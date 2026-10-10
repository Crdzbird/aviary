#include "aviary/pow_engine.hpp"

#include <algorithm>
#include <array>
#include <string>

namespace aviary::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

// The 256-bit target as four big-endian 64-bit limbs (limb 0 = most significant).
std::array<std::uint64_t, 4> to_limbs(const Hash& h) {
  std::array<std::uint64_t, 4> l{};
  for (int i = 0; i < 4; ++i) {
    std::uint64_t v = 0;
    for (int j = 0; j < 8; ++j) v = (v << 8) | h[i * 8 + j];
    l[i] = v;
  }
  return l;
}

Hash from_limbs(const std::array<std::uint64_t, 4>& l) {
  Hash h{};
  for (int i = 0; i < 4; ++i) {
    std::uint64_t v = l[i];
    for (int j = 7; j >= 0; --j) {
      h[i * 8 + j] = static_cast<std::uint8_t>(v & 0xFF);
      v >>= 8;
    }
  }
  return h;
}

// (256-bit target × m) / d, big-endian, saturating to all-ones on overflow.
Hash mul_div_target(const Hash& target, std::uint64_t m, std::uint64_t d) {
  const auto l = to_limbs(target);
  // 256 × 64 → up to 320-bit product in five limbs (p[0] most significant).
  std::array<std::uint64_t, 5> p{};
  unsigned __int128 carry = 0;
  for (int i = 3; i >= 0; --i) {
    const unsigned __int128 cur = static_cast<unsigned __int128>(l[i]) * m + carry;
    p[i + 1] = static_cast<std::uint64_t>(cur);
    carry = cur >> 64;
  }
  p[0] = static_cast<std::uint64_t>(carry);
  // Long-divide the 320-bit value by d.
  std::array<std::uint64_t, 5> q{};
  unsigned __int128 rem = 0;
  for (int i = 0; i < 5; ++i) {
    const unsigned __int128 acc = (rem << 64) | p[i];
    q[i] = static_cast<std::uint64_t>(acc / d);
    rem = acc % d;
  }
  if (q[0] != 0) {  // result exceeds 256 bits → clamp to the easiest possible target
    Hash mx;
    mx.fill(0xFF);
    return mx;
  }
  return from_limbs({q[1], q[2], q[3], q[4]});
}
}  // namespace

Hash PowEngine::retarget(const Hash& current, std::uint64_t actual_interval,
                         std::uint64_t desired_interval) {
  if (desired_interval == 0) return current;
  // Clamp the adjustment ratio to [1/4, 4] so difficulty can't swing wildly.
  const std::uint64_t lo = std::max<std::uint64_t>(1, desired_interval / 4);
  const std::uint64_t hi = desired_interval * 4;  // callers keep desired small enough
  const std::uint64_t a = std::clamp(actual_interval, lo, hi);
  return mul_div_target(current, a, desired_interval);
}

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
  return propose_anchored(std::move(txs), timestamp, {});
}
Block PowEngine::propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                                  std::vector<Anchor> anchors) {
  Block b = ledger_->build_block(std::move(txs), timestamp, std::move(anchors));
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

}  // namespace aviary::consensus
