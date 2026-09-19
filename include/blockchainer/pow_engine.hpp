#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/types.hpp"

/// A proof-of-work reference engine (ARCHITECTURE §7), demonstrating a second
/// consensus family behind the same [ConsensusEngine] seam as the BFT engine.
///
/// A block is justified by *work*: its header hash must not exceed a difficulty
/// [target]. The nonce lives in `consensus_data` (so it is committed by the
/// header hash), which is exactly what a miner varies. `propose` returns an
/// unsolved candidate; `mine` searches for a nonce that meets the target; and
/// `submit` re-verifies the embedded work before committing — the work is
/// intrinsic to the block, so `submit`'s justification argument is unused.
///
/// This is a *reference* engine: the difficulty target is fixed (no retargeting)
/// and the chain extends linearly (no longest-chain fork choice / reorg). Those
/// are larger follow-ups; the point here is that an entirely different consensus
/// family slots in under the interface without touching the layers above it.
namespace blockchainer::consensus {

/// The proof-of-work payload carried in a block's consensus_data.
struct PowData {
  Hash target{};             ///< the block hash must be <= this
  std::uint64_t nonce = 0;   ///< the miner's search variable

  Bytes encode() const;
  static PowData decode_all(codec::ByteView bytes);
};

class PowEngine : public ConsensusEngine {
 public:
  /// [target]: a block's header hash must be <= this (more leading zero bytes =
  /// harder). All produced/accepted blocks use this fixed target.
  PowEngine(Hash target, std::unique_ptr<Ledger> ledger)
      : target_(target), ledger_(std::move(ledger)) {}

  const Ledger& ledger() const override { return *ledger_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }
  const Hash& target() const { return target_; }

  /// Assemble an UNSOLVED candidate block (nonce 0); call mine() to solve it.
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) override;
  Block propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                         std::vector<Anchor> anchors) override;

  /// Verify the block's embedded proof of work, then apply it. The justification
  /// argument is unused (the proof is intrinsic to the block).
  BlockError submit(const Block& block, codec::ByteView justification) override;

  /// Search nonces (in-place, in [block].header.consensus_data) until the block
  /// hash meets the target, up to [max_iters]. Returns true if solved.
  bool mine(Block& block, std::uint64_t max_iters = 1u << 24) const;

  /// Whether [block_hash] satisfies [target] (hash <= target, big-endian).
  static bool meets_target(const Hash& block_hash, const Hash& target) {
    return block_hash <= target;
  }

  /// Adjust the difficulty so mining tracks [desired_interval] given the
  /// [actual_interval] the last block(s) took: new_target ≈ current × actual /
  /// desired. Blocks that came too SLOW (actual > desired) make the target
  /// LARGER (easier); too fast makes it smaller (harder). The per-step change is
  /// clamped to [1/4, 4]× (Bitcoin-style) and saturates at the all-ones max.
  static Hash retarget(const Hash& current, std::uint64_t actual_interval,
                       std::uint64_t desired_interval);

  /// Install a new difficulty target (e.g. the result of retarget()).
  void set_target(const Hash& target) { target_ = target; }

 private:
  Hash target_;
  std::unique_ptr<Ledger> ledger_;
};

}  // namespace blockchainer::consensus
