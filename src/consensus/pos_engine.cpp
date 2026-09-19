#include "blockchainer/pos_engine.hpp"

#include <stdexcept>
#include <string>

namespace blockchainer::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

// Domain byte for a PoS proposal signature — distinct from BFT precommit votes
// (0x01) so a vote signature can never be replayed as a proposal.
constexpr std::uint8_t kPosProposal = 0x02;
}  // namespace

std::uint32_t stake_weighted_proposer(std::uint64_t height, const ValidatorSet& vset) {
  const std::uint64_t total = vset.total_power();
  if (total == 0 || vset.size() == 0) return 0;

  // Deterministic ticket in [0, total) derived from the height.
  codec::CodecWriter w;
  w.write_u64(height);
  const Hash h = crypto::hash_tagged(crypto::Domain::kProposer, w.take());
  std::uint64_t ticket = 0;
  for (int i = 0; i < 8; ++i) ticket = (ticket << 8) | h[i];
  ticket %= total;

  // Walk validators, accumulating stake — the ticket lands in the leader's band.
  // Zero-power validators occupy no band, so they are never selected.
  std::uint64_t acc = 0;
  for (std::uint32_t i = 0; i < vset.size(); ++i) {
    acc += vset.at(i).power;
    if (ticket < acc) return i;
  }
  return static_cast<std::uint32_t>(vset.size() - 1);  // unreachable when total > 0
}

Bytes PosData::encode() const {
  codec::CodecWriter w;
  w.write_u32(proposer_index);
  return w.take();
}

PosData PosData::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  PosData d;
  d.proposer_index = r.read_u32();
  r.assert_fully_consumed();
  return d;
}

Bytes pos_signing_bytes(std::uint64_t chain_id, std::uint64_t height, const Hash& block_id) {
  codec::CodecWriter w;
  w.write_u8(kPosProposal);
  w.write_u64(chain_id);
  w.write_u64(height);
  w.write_fixed_bytes(hview(block_id), 32);
  return w.take();
}

Block PosEngine::propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
  return propose_anchored(std::move(txs), timestamp, {});
}
Block PosEngine::propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                                  std::vector<Anchor> anchors) {
  const std::uint64_t next = ledger_->tip().height + 1;
  if (proposer_for(next) != me_) {
    throw std::runtime_error("pos: this validator is not the leader for height " +
                             std::to_string(next));
  }
  Block b = ledger_->build_block(std::move(txs), timestamp, std::move(anchors));
  b.header.consensus_data = PosData{me_}.encode();
  return b;
}

Bytes PosEngine::seal(const Block& block) const {
  const Bytes msg = pos_signing_bytes(ledger_->spec().chain_id, block.header.height,
                                      block.header.hash());
  return crypto::bls_sign(view(msg), view(key_.secret_key));
}

BlockError PosEngine::submit(const Block& block, codec::ByteView justification) {
  PosData d;
  try {
    d = PosData::decode_all(view(block.header.consensus_data));
  } catch (const codec::CodecError& e) {
    return BlockError::failure(std::string("pos: malformed proposer data: ") + e.what());
  }
  if (d.proposer_index >= vset_.size()) {
    return BlockError::failure("pos: proposer index out of range");
  }
  // The proposer must be the legitimate stake-weighted leader for this height.
  if (d.proposer_index != proposer_for(block.header.height)) {
    return BlockError::failure("pos: block not produced by the height's leader");
  }
  // ...and the justification must be that leader's signature over the block id.
  const Bytes msg = pos_signing_bytes(ledger_->spec().chain_id, block.header.height,
                                      block.header.hash());
  if (!crypto::bls_verify(view(msg), justification, view(vset_.at(d.proposer_index).bls_pubkey))) {
    return BlockError::failure("pos: invalid leader signature");
  }
  return ledger_->process_block(block);
}

}  // namespace blockchainer::consensus
