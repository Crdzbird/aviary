#include "blockchainer/ledger.hpp"

#include <stdexcept>

namespace blockchainer {

Ledger::Ledger(ChainSpec spec, std::unique_ptr<StateMachine> genesis_state,
               std::uint64_t genesis_timestamp)
    : spec_(spec), state_(std::move(genesis_state)) {
  tip_.version = 1;
  tip_.chain_id = spec_.chain_id;
  tip_.height = 0;
  tip_.parent_hash = crypto::zero_hash();
  tip_.timestamp = genesis_timestamp;
  tip_.tx_root = compute_tx_root({});
  tip_.state_root = state_->state_root();
}

BlockError Ledger::check_header(const BlockHeader& h) const {
  if (h.chain_id != spec_.chain_id) return BlockError::failure("chain_id mismatch");
  if (h.height != tip_.height + 1) return BlockError::failure("non-sequential height");
  if (h.parent_hash != tip_.hash()) return BlockError::failure("parent hash mismatch");
  if (h.timestamp < tip_.timestamp) return BlockError::failure("non-monotonic timestamp");
  return BlockError::success();
}

Block Ledger::build_block(std::vector<Transaction> txs, std::uint64_t timestamp) {
  auto working = state_->clone();
  for (const auto& tx : txs) {
    if (tx.chain_id != spec_.chain_id) {
      throw std::runtime_error("build_block: tx chain_id mismatch");
    }
    if (!spec_.accepts(tx.algo())) {
      throw std::runtime_error("build_block: signature scheme not accepted by chain");
    }
    if (!verify_transaction_signature(tx)) {
      throw std::runtime_error("build_block: bad signature");
    }
    const ValidationResult vr = working->validate(tx);
    if (!vr.ok) throw std::runtime_error("build_block: invalid tx: " + vr.error);
    working->apply(tx);
  }

  Block block;
  block.header.version = 1;
  block.header.chain_id = spec_.chain_id;
  block.header.height = tip_.height + 1;
  block.header.parent_hash = tip_.hash();
  block.header.timestamp = timestamp;
  block.header.tx_root = compute_tx_root(txs);
  block.header.state_root = working->state_root();
  block.transactions = std::move(txs);
  return block;
}

BlockError Ledger::process_block(const Block& block) {
  // 2. Header checks (1. decode happened to construct the Block).
  if (BlockError he = check_header(block.header); !he.ok) return he;

  // 3. Recompute and compare the transaction root.
  if (compute_tx_root(block.transactions) != block.header.tx_root) {
    return BlockError::failure("tx_root mismatch");
  }

  // 4. Per-tx: signature verify → state validate → apply, on a working copy so
  // the commit is all-or-nothing.
  auto working = state_->clone();
  for (const auto& tx : block.transactions) {
    if (tx.chain_id != spec_.chain_id) {
      return BlockError::failure("tx chain_id mismatch");
    }
    if (!spec_.accepts(tx.algo())) {
      return BlockError::failure("signature scheme not accepted by chain");
    }
    if (!verify_transaction_signature(tx)) {
      return BlockError::failure("invalid tx signature");
    }
    const ValidationResult vr = working->validate(tx);
    if (!vr.ok) return BlockError::failure("invalid tx: " + vr.error);
    working->apply(tx);
  }

  // 5. Recompute and compare the post-state root.
  if (working->state_root() != block.header.state_root) {
    return BlockError::failure("state_root mismatch");
  }

  // 6. (consensus verification — M2)
  // 7. Commit atomically.
  state_ = std::move(working);
  tip_ = block.header;
  return BlockError::success();
}

}  // namespace blockchainer
