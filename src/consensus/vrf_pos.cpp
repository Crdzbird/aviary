#include <memory>
#include <optional>
#include "aviary/vrf_pos.hpp"

#include <stdexcept>
#include <string>

#include "aviary/pos_engine.hpp"  // pos_signing_bytes — same sealing rule

namespace aviary::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

// Domain byte for the VRF eligibility input — distinct from BFT votes (0x01)
// and PoS proposal seals (0x02), so a proof can never double as either.
constexpr std::uint8_t kVrfEligibility = 0x03;
}  // namespace

Bytes VrfPosData::encode() const {
  codec::CodecWriter w;
  w.write_u8(1);  // VrfPosData format version
  w.write_u32(proposer_index);
  w.write_u32(attempt);
  w.write_bytes(view(vrf_proof));
  return w.take();
}

VrfPosData VrfPosData::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  if (r.read_u8() != 1) throw codec::CodecError("unsupported VrfPosData version");
  VrfPosData d;
  d.proposer_index = r.read_u32();
  d.attempt = r.read_u32();
  d.vrf_proof = r.read_bytes();
  r.assert_fully_consumed();
  return d;
}

VrfPosEngine::VrfPosEngine(ValidatorSet vset, std::vector<Bytes> vrf_pubkeys, std::uint32_t me,
                           crypto::KeyPair key, Bytes vrf_secret, std::unique_ptr<Ledger> ledger,
                           std::uint32_t oversample)
    : vset_(std::move(vset)),
      vrf_pubkeys_(std::move(vrf_pubkeys)),
      me_(me),
      key_(std::move(key)),
      vrf_secret_(std::move(vrf_secret)),
      ledger_(std::move(ledger)),
      oversample_(oversample) {
  if (vrf_pubkeys_.size() != vset_.size()) {
    throw std::runtime_error("vrf-pos: vrf key registry must align with the validator set");
  }
  if (oversample_ == 0) throw std::runtime_error("vrf-pos: oversample must be >= 1");
}

Bytes VrfPosEngine::alpha(std::uint64_t height, std::uint32_t attempt, const Hash& parent) const {
  codec::CodecWriter w;
  w.write_u8(kVrfEligibility);
  w.write_u64(ledger_->spec().chain_id);
  w.write_u64(height);
  w.write_u32(attempt);
  w.write_fixed_bytes(hview(parent), 32);
  return w.take();
}

bool VrfPosEngine::meets_threshold(const Bytes& beta, std::uint32_t index) const {
  const std::uint64_t total = vset_.total_power();
  if (total == 0 || beta.size() < 8) return false;
  const std::uint64_t power = vset_.at(index).power;
  if (power == 0) return false;  // slashed validators are never eligible

  // Ticket uniform in [0, total); eligible band = min(oversample × power, total).
  // (unsigned __int128 avoids overflow; power × oversample can exceed 2^64.)
  std::uint64_t ticket = 0;
  for (int i = 0; i < 8; ++i) ticket = (ticket << 8) | beta[i];
  ticket %= total;
  const unsigned __int128 band = static_cast<unsigned __int128>(power) * oversample_;
  return ticket < (band >= total ? total : static_cast<std::uint64_t>(band));
}

std::optional<Bytes> VrfPosEngine::eligible(std::uint32_t attempt) const {
  const std::uint64_t next = ledger_->tip().height + 1;
  const Bytes a = alpha(next, attempt, ledger_->tip().hash());
  const Bytes proof = crypto::ecvrf_prove(view(vrf_secret_), view(a));
  const Bytes beta = crypto::ecvrf_proof_to_hash(view(proof));
  if (!meets_threshold(beta, me_)) return std::nullopt;
  return proof;
}

Block VrfPosEngine::propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
  return propose_anchored(std::move(txs), timestamp, {});
}
Block VrfPosEngine::propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                                     std::vector<Anchor> anchors) {
  for (std::uint32_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
    std::optional<Bytes> proof = eligible(attempt);
    if (!proof) continue;
    Block b = ledger_->build_block(std::move(txs), timestamp, std::move(anchors));
    b.header.consensus_data = VrfPosData{me_, attempt, std::move(*proof)}.encode();
    return b;
  }
  throw std::runtime_error("vrf-pos: this validator is not eligible at any attempt for height " +
                           std::to_string(ledger_->tip().height + 1));
}

Bytes VrfPosEngine::seal(const Block& block) const {
  const Bytes msg = pos_signing_bytes(ledger_->spec().chain_id, block.header.height,
                                      block.header.hash());
  return crypto::bls_sign(view(msg), view(key_.secret_key));
}

BlockError VrfPosEngine::submit(const Block& block, codec::ByteView justification) {
  VrfPosData d;
  try {
    d = VrfPosData::decode_all(view(block.header.consensus_data));
  } catch (const codec::CodecError& e) {
    return BlockError::failure(std::string("vrf-pos: malformed consensus data: ") + e.what());
  }
  if (d.proposer_index >= vset_.size()) {
    return BlockError::failure("vrf-pos: proposer index out of range");
  }
  if (d.attempt >= kMaxAttempts) {
    return BlockError::failure("vrf-pos: attempt out of range");
  }

  // The proof must verify under the claimed proposer's REGISTERED VRF key, over
  // the alpha this exact block position implies (height + parent pin it to this
  // tip — a proof for another height/branch cannot be replayed here)...
  const Bytes a = alpha(block.header.height, d.attempt, block.header.parent_hash);
  const std::optional<Bytes> beta =
      crypto::ecvrf_verify(view(vrf_pubkeys_[d.proposer_index]), view(a), view(d.vrf_proof));
  if (!beta) return BlockError::failure("vrf-pos: invalid eligibility proof");
  // ...and its output must actually clear the proposer's stake threshold.
  if (!meets_threshold(*beta, d.proposer_index)) {
    return BlockError::failure("vrf-pos: proof does not meet the stake threshold");
  }

  // The justification is the eligible leader's signature over the block id.
  const Bytes msg = pos_signing_bytes(ledger_->spec().chain_id, block.header.height,
                                      block.header.hash());
  if (!crypto::bls_verify(view(msg), justification, view(vset_.at(d.proposer_index).bls_pubkey))) {
    return BlockError::failure("vrf-pos: invalid leader signature");
  }
  return ledger_->process_block(block);
}

}  // namespace aviary::consensus
