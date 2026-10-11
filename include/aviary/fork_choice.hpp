#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "aviary/ledger.hpp"
#include "aviary/state.hpp"
#include "aviary/types.hpp"

/// Fork choice + reorg (ARCHITECTURE section 6/section 7). The [Ledger] is a single linear
/// chain; a real PoW/PoS network sees a *tree* of competing blocks and must pick
/// the canonical branch and reorganize when a heavier one appears. [ForkChoice]
/// is that tree: it stores blocks by hash, tracks each block's cumulative
/// fork-choice weight, keeps the canonical head on the heaviest branch, and
/// rebuilds the ledger state on a reorg by replaying the winning branch.
///
/// It is **engine-agnostic**: the per-block weight is supplied by the caller, so
/// the fork-choice rule is decoupled from consensus - a PoW engine passes the
/// block's work, a PoS/BFT longest-chain rule passes 1. The tree validates only
/// the STATE transition (via the Ledger); the consensus *justification* (a BFT
/// certificate, a PoW nonce, a PoS leader signature) is verified by the engine
/// before a block is offered here.
namespace aviary {

class ForkChoice {
 public:
  /// [genesis_state] must deterministically reproduce the genesis state each call
  /// (used to replay branches). The genesis header hash is derived from it.
  ForkChoice(ChainSpec spec, std::function<std::unique_ptr<StateMachine>()> genesis_state,
             std::uint64_t genesis_timestamp = 0);

  struct AddResult {
    bool accepted = false;   ///< the block was a valid extension and was stored
    bool reorg = false;      ///< accepting it moved the head to a different branch
    std::string error;       ///< why it was rejected (when !accepted)
  };

  /// Offer [block] with its fork-choice [weight] (must be >= 1). It must extend a
  /// known parent (a stored block or genesis) and be a valid state transition on
  /// that parent. On success it is stored; if its branch becomes the heaviest,
  /// the canonical head advances (a reorg if it was on a different branch).
  AddResult add_block(const Block& block, std::uint64_t weight = 1);

  /// The canonical tip block hash (the genesis hash when empty).
  const Hash& head() const { return head_; }
  std::uint64_t head_height() const { return canonical_->tip().height; }
  bool knows(const Hash& block_hash) const { return nodes_.count(block_hash) != 0; }
  std::size_t size() const { return nodes_.size(); }

  /// The ledger at the canonical head.
  const Ledger& canonical() const { return *canonical_; }

 private:
  struct Node {
    Block block;
    Hash parent{};
    std::uint64_t height = 0;
    std::uint64_t cumulative_weight = 0;
  };

  std::uint64_t weight_of(const Hash& head) const;  // cumulative weight (0 at genesis)
  std::unique_ptr<Ledger> ledger_at(const Hash& block_hash) const;  // replay genesis->hash

  ChainSpec spec_;
  std::function<std::unique_ptr<StateMachine>()> genesis_state_;
  std::uint64_t genesis_timestamp_;
  Hash genesis_hash_{};

  std::map<Hash, Node> nodes_;
  Hash head_{};
  std::unique_ptr<Ledger> canonical_;  // ledger at head_
};

}  // namespace aviary
