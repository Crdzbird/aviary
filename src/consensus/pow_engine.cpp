#include "blockchainer/pow_engine.hpp"

#include <string>

namespace blockchainer::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}
}  // namespace

Bytes PowData::encode() const {
  codec::CodecWriter w;
  w.write_fixed_bytes(hview(target), 32);
  w.write_u64(nonce);
  return w.take();
}

PowData PowData::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  PowData d;
  d.target = to_hash(r.read_fixed_bytes(32));
  d.nonce = r.read_u64();
  r.assert_fully_consumed();
  return d;
}

Block PowEngine::propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
  Block b = ledger_->build_block(std::move(txs), timestamp);
  b.header.consensus_data = PowData{target_, 0}.encode();  // unsolved candidate
  return b;
}

bool PowEngine::mine(Block& block, std::uint64_t max_iters) const {
  PowData d{target_, 0};
  for (std::uint64_t nonce = 0; nonce < max_iters; ++nonce) {
    d.nonce = nonce;
    block.header.consensus_data = d.encode();
    if (meets_target(block.header.hash(), target_)) return true;
  }
  return false;
}

BlockError PowEngine::submit(const Block& block, codec::ByteView /*justification*/) {
  PowData d;
  try {
    d = PowData::decode_all(view(block.header.consensus_data));
  } catch (const codec::CodecError& e) {
    return BlockError::failure(std::string("pow: malformed work data: ") + e.what());
  }
  if (d.target != target_) {
    return BlockError::failure("pow: wrong difficulty target");
  }
  if (!meets_target(block.header.hash(), target_)) {
    return BlockError::failure("pow: insufficient proof of work");
  }
  // The header hash met the target; process_block verifies the state transition
  // and commits atomically.
  return ledger_->process_block(block);
}

}  // namespace blockchainer::consensus
