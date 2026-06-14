#include "blockchainer/consensus.hpp"

#include <algorithm>
#include <set>

namespace blockchainer::consensus {

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
  for (const auto& v : validators_) total_power_ += v.power;
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
  return crypto::hash_tagged(crypto::Domain::kHeader, w.take());
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
  std::vector<std::uint32_t> signers;
  std::vector<Bytes> sigs;
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
    signers.push_back(v.validator_index);
    sigs.push_back(v.signature);
    power += vset.at(v.validator_index).power;
  }

  if (power < vset.quorum()) return std::nullopt;

  std::sort(signers.begin(), signers.end());
  // sigs were pushed in signers' pre-sort order; re-derive sorted sigs.
  std::vector<Bytes> sorted_sigs;
  sorted_sigs.reserve(signers.size());
  for (std::uint32_t idx : signers) {
    for (const auto& v : votes) {
      if (v.validator_index == idx) {
        sorted_sigs.push_back(v.signature);
        break;
      }
    }
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

}  // namespace blockchainer::consensus
