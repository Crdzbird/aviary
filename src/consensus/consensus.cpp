#include <optional>
#include "aviary/consensus.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
#include <utility>

namespace aviary::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

// Domain tag distinguishing a precommit from any other signed message.
constexpr std::uint8_t kPrecommit = 0x01;
}  // namespace

ValidatorSet::ValidatorSet(std::vector<Validator> validators)
    : validators_(std::move(validators)) {
  // Reject malformed sets at construction — a bad set is a consensus footgun.
  // (A zero-power *validator* is allowed: that is exactly a slashed validator;
  // what is rejected is a set with no live power at all.)
  if (validators_.empty()) throw std::invalid_argument("ValidatorSet: empty set");
  std::set<Bytes> seen;
  for (const auto& v : validators_) {
    if (v.bls_pubkey.size() != 48) {
      throw std::invalid_argument("ValidatorSet: BLS public key must be 48 bytes");
    }
    if (!seen.insert(v.bls_pubkey).second) {
      throw std::invalid_argument("ValidatorSet: duplicate validator public key");
    }
    total_power_ += v.power;
  }
  if (total_power_ == 0) throw std::invalid_argument("ValidatorSet: total voting power is zero");
}

std::uint64_t ValidatorSet::quorum() const {
  // Strictly greater than 2/3 of the total power.
  return (total_power_ * 2) / 3 + 1;
}

Hash ValidatorSet::hash() const {
  codec::CodecWriter w;
  w.write_list(validators_, [](codec::CodecWriter& w, const Validator& v) {
    w.write_bytes(view(v.bls_pubkey));
    w.write_u64(v.power);
  });
  return crypto::hash_tagged(crypto::Domain::kValidatorSet, w.take());
}

ValidatorSet ValidatorSet::with_slashed(std::uint32_t index) const {
  std::vector<Validator> next = validators_;
  if (index < next.size()) next[index].power = 0;
  return ValidatorSet(std::move(next));
}

Bytes ValidatorSet::encode() const {
  codec::CodecWriter w;
  w.write_list(validators_, [](codec::CodecWriter& w, const Validator& v) {
    w.write_bytes(view(v.bls_pubkey));
    w.write_u64(v.power);
  });
  return w.take();
}

ValidatorSet ValidatorSet::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  auto vs = r.read_list<Validator>([](codec::CodecReader& r) {
    Validator v;
    v.bls_pubkey = r.read_bytes();
    v.power = r.read_u64();
    return v;
  });
  r.assert_fully_consumed();
  return ValidatorSet(std::move(vs));
}

Bytes vote_signing_bytes(std::uint64_t chain_id, std::uint64_t height,
                         std::uint32_t round, const Hash& block_id,
                         std::uint32_t validator_index) {
  codec::CodecWriter w;
  w.write_u8(kPrecommit);
  w.write_u64(chain_id);
  w.write_u64(height);
  w.write_u32(round);
  w.write_fixed_bytes(view(block_id), 32);
  w.write_u32(validator_index);
  return w.take();
}

Bytes CommitCertificate::encode() const {
  codec::CodecWriter w;
  w.write_u64(height);
  w.write_u32(round);
  w.write_fixed_bytes(view(block_id), 32);
  w.write_list(signers, [](codec::CodecWriter& w, std::uint32_t s) { w.write_u32(s); });
  w.write_bytes(view(agg_signature));
  return w.take();
}

CommitCertificate CommitCertificate::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  CommitCertificate c;
  c.height = r.read_u64();
  c.round = r.read_u32();
  c.block_id = to_hash(r.read_fixed_bytes(32));
  c.signers = r.read_list<std::uint32_t>([](codec::CodecReader& r) { return r.read_u32(); });
  c.agg_signature = r.read_bytes();
  r.assert_fully_consumed();
  return c;
}

Vote sign_vote(std::uint64_t chain_id, std::uint64_t height, std::uint32_t round,
               const Hash& block_id, std::uint32_t validator_index,
               const crypto::KeyPair& bls_key) {
  Bytes msg = vote_signing_bytes(chain_id, height, round, block_id, validator_index);
  Vote v;
  v.validator_index = validator_index;
  v.signature = crypto::bls_sign(view(msg), view(bls_key.secret_key));
  return v;
}

std::optional<CommitCertificate> aggregate_commit(
    std::uint64_t chain_id, const ValidatorSet& vset, std::uint64_t height,
    std::uint32_t round, const Hash& block_id, const std::vector<Vote>& votes) {
  std::set<std::uint32_t> included;
  std::vector<std::pair<std::uint32_t, Bytes>> collected;  // (index, signature)
  std::uint64_t power = 0;

  for (const auto& v : votes) {
    if (v.validator_index >= vset.size()) continue;
    if (!included.insert(v.validator_index).second) continue;  // dedupe
    const Bytes msg =
        vote_signing_bytes(chain_id, height, round, block_id, v.validator_index);
    if (!crypto::bls_verify(view(msg), view(v.signature),
                            view(vset.at(v.validator_index).bls_pubkey))) {
      included.erase(v.validator_index);
      continue;  // drop invalid vote
    }
    collected.emplace_back(v.validator_index, v.signature);
    power += vset.at(v.validator_index).power;
  }

  if (power < vset.quorum()) return std::nullopt;

  // Sort (index, signature) together so signers ascend and the aggregated
  // signatures stay aligned — O(n log n), no per-signer re-scan.
  std::sort(collected.begin(), collected.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  std::vector<std::uint32_t> signers;
  std::vector<Bytes> sorted_sigs;
  signers.reserve(collected.size());
  sorted_sigs.reserve(collected.size());
  for (auto& [idx, sig] : collected) {
    signers.push_back(idx);
    sorted_sigs.push_back(std::move(sig));
  }

  CommitCertificate cert;
  cert.height = height;
  cert.round = round;
  cert.block_id = block_id;
  cert.signers = std::move(signers);
  cert.agg_signature = crypto::bls_aggregate(sorted_sigs);
  return cert;
}

CommitError verify_commit(std::uint64_t chain_id, const ValidatorSet& vset,
                          const CommitCertificate& cert) {
  if (cert.signers.empty()) return CommitError::failure("no signers");

  // Signers must be strictly ascending (→ unique) and in range.
  std::uint64_t power = 0;
  std::vector<Bytes> msgs, pks;
  msgs.reserve(cert.signers.size());
  pks.reserve(cert.signers.size());
  for (std::size_t i = 0; i < cert.signers.size(); ++i) {
    const std::uint32_t idx = cert.signers[i];
    if (i > 0 && idx <= cert.signers[i - 1]) {
      return CommitError::failure("signers not strictly ascending");
    }
    if (idx >= vset.size()) return CommitError::failure("signer index out of range");
    power += vset.at(idx).power;
    msgs.push_back(vote_signing_bytes(chain_id, cert.height, cert.round,
                                      cert.block_id, idx));
    pks.push_back(vset.at(idx).bls_pubkey);
  }

  if (power < vset.quorum()) return CommitError::failure("below quorum power");

  if (!crypto::bls_aggregate_verify(msgs, pks, view(cert.agg_signature))) {
    return CommitError::failure("invalid aggregate signature");
  }
  return CommitError::success();
}

// ── Consensus header payload (S4) ───────────────────────────────────────────

std::optional<std::uint32_t> ValidatorSet::index_of(const Bytes& bls_pubkey) const {
  for (std::size_t i = 0; i < validators_.size(); ++i)
    if (validators_[i].bls_pubkey == bls_pubkey) return static_cast<std::uint32_t>(i);
  return std::nullopt;
}

Bytes ConsensusData::encode() const {
  codec::CodecWriter w;
  w.write_fixed_bytes(view(validator_set_hash), 32);
  w.write_u8(last_commit ? 1 : 0);
  if (last_commit) w.write_bytes(view(last_commit->encode()));
  w.write_u8(next_validators ? 1 : 0);
  if (next_validators) {
    w.write_list(*next_validators, [](codec::CodecWriter& w, const Validator& v) {
      w.write_bytes(view(v.bls_pubkey));
      w.write_u64(v.power);
    });
  }
  return w.take();
}

ConsensusData ConsensusData::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  ConsensusData cd;
  cd.validator_set_hash = to_hash(r.read_fixed_bytes(32));
  if (r.read_u8() != 0) {
    cd.last_commit = CommitCertificate::decode_all(view(r.read_bytes()));
  }
  if (r.read_u8() != 0) {
    cd.next_validators = r.read_list<Validator>([](codec::CodecReader& r) {
      Validator v;
      v.bls_pubkey = r.read_bytes();
      v.power = r.read_u64();
      return v;
    });
  }
  r.assert_fully_consumed();
  return cd;
}

std::optional<ValidatorSet> announced_validator_set(const BlockHeader& h) {
  try {
    ConsensusData cd = ConsensusData::decode_all(view(h.consensus_data));
    if (!cd.next_validators) return std::nullopt;
    return ValidatorSet(*cd.next_validators);
  } catch (...) {
    return std::nullopt;
  }
}

CommitError validate_consensus_data(std::uint64_t chain_id, const ValidatorSet& vset,
                                    const BlockHeader& h, const Hash& parent_hash) {
  return validate_consensus_data(chain_id, vset, vset, h, parent_hash);
}

CommitError validate_consensus_data(std::uint64_t chain_id, const ValidatorSet& current,
                                    const ValidatorSet& certifying, const BlockHeader& h,
                                    const Hash& parent_hash) {
  ConsensusData cd;
  try {
    cd = ConsensusData::decode_all(view(h.consensus_data));
  } catch (...) {
    return CommitError::failure("malformed consensus data");
  }
  if (cd.validator_set_hash != current.hash()) {
    return CommitError::failure("validator-set hash mismatch");
  }
  if (cd.next_validators) {
    try {
      (void)ValidatorSet(*cd.next_validators);  // well-formed: non-empty, 48-byte unique keys, live power
    } catch (const std::exception& e) {
      return CommitError::failure(std::string("invalid next validator set: ") + e.what());
    }
  }
  const ValidatorSet& vset = certifying;  // verifies the parent's commit
  // The first block after genesis (height 1) has no finalized parent to certify.
  if (h.height <= 1) {
    if (cd.last_commit) return CommitError::failure("first block must not carry a last commit");
    return CommitError::success();
  }
  if (!cd.last_commit) return CommitError::failure("missing last commit");
  const CommitCertificate& lc = *cd.last_commit;
  if (lc.height != h.height - 1) return CommitError::failure("last-commit height mismatch");
  if (lc.block_id != parent_hash) {
    return CommitError::failure("last commit does not certify the parent");
  }
  return verify_commit(chain_id, vset, lc);
}

// ── Equivocation evidence (S4) ──────────────────────────────────────────────

Bytes Evidence::encode() const {
  codec::CodecWriter w;
  w.write_u32(validator_index);
  w.write_u64(height);
  w.write_u32(round);
  w.write_fixed_bytes(view(block_id_a), 32);
  w.write_bytes(view(signature_a));
  w.write_fixed_bytes(view(block_id_b), 32);
  w.write_bytes(view(signature_b));
  return w.take();
}

Evidence Evidence::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Evidence ev;
  ev.validator_index = r.read_u32();
  ev.height = r.read_u64();
  ev.round = r.read_u32();
  ev.block_id_a = to_hash(r.read_fixed_bytes(32));
  ev.signature_a = r.read_bytes();
  ev.block_id_b = to_hash(r.read_fixed_bytes(32));
  ev.signature_b = r.read_bytes();
  r.assert_fully_consumed();
  return ev;
}

CommitError verify_equivocation(std::uint64_t chain_id, const ValidatorSet& vset,
                                const Evidence& ev) {
  if (ev.validator_index >= vset.size()) {
    return CommitError::failure("signer index out of range");
  }
  if (ev.block_id_a == ev.block_id_b) {
    return CommitError::failure("not conflicting: same block id");
  }
  const Bytes pk = vset.at(ev.validator_index).bls_pubkey;
  const Bytes msg_a =
      vote_signing_bytes(chain_id, ev.height, ev.round, ev.block_id_a, ev.validator_index);
  const Bytes msg_b =
      vote_signing_bytes(chain_id, ev.height, ev.round, ev.block_id_b, ev.validator_index);
  if (!crypto::bls_verify(view(msg_a), view(ev.signature_a), view(pk))) {
    return CommitError::failure("first signature invalid");
  }
  if (!crypto::bls_verify(view(msg_b), view(ev.signature_b), view(pk))) {
    return CommitError::failure("second signature invalid");
  }
  return CommitError::success();  // proven double-sign
}

std::optional<Evidence> detect_equivocation(std::uint64_t height, std::uint32_t round,
                                            const std::vector<AttributedVote>& votes) {
  std::map<std::uint32_t, AttributedVote> seen;  // first vote per validator
  for (const auto& av : votes) {
    auto [it, fresh] = seen.emplace(av.vote.validator_index, av);
    if (fresh) continue;
    if (it->second.block_id == av.block_id) continue;  // same block → not a conflict
    Evidence ev;
    ev.validator_index = av.vote.validator_index;
    ev.height = height;
    ev.round = round;
    ev.block_id_a = it->second.block_id;
    ev.signature_a = it->second.vote.signature;
    ev.block_id_b = av.block_id;
    ev.signature_b = av.vote.signature;
    return ev;
  }
  return std::nullopt;
}

}  // namespace aviary::consensus
