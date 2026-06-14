#include "blockchainer/mempool.hpp"

#include <set>

#include "blockchainer/observability/event_bus.hpp"
#include "blockchainer/observability/events.hpp"

namespace blockchainer {

AddResult Mempool::add(const Transaction& tx, std::uint64_t chain_id) {
  if (tx.chain_id != chain_id) return AddResult::kWrongChain;

  const Bytes enc = tx.encode();
  if (enc.size() > cfg_.max_tx_bytes) return AddResult::kTooLarge;

  const Hash id = tx.txid();
  if (pool_.count(id) != 0) return AddResult::kDuplicate;

  if (!verify_transaction_signature(tx)) return AddResult::kInvalidSignature;

  // Capacity: reject when full (no fee market yet → no priority eviction; see
  // docs/IMPROVEMENTS.md).
  if (pool_.size() >= cfg_.max_txs) return AddResult::kFull;

  pool_.emplace(id, tx);
  if (bus_) bus_->publish(observe::TxAdmitted{id, pool_.size()});
  return AddResult::kAdded;
}

std::vector<Transaction> Mempool::select(const StateMachine& state,
                                         std::size_t max) const {
  auto working = state.clone();
  std::vector<Transaction> out;
  std::set<Hash> used;

  // Fixed-point: repeatedly apply any pending tx that validates against the
  // evolving state. A tx that depends on another (account nonce n needs n-1
  // applied; a UTXO spending a not-yet-created output) becomes ready in a later
  // pass. Iteration over pool_ (txid order) makes selection deterministic.
  bool progress = true;
  while (progress && out.size() < max) {
    progress = false;
    for (const auto& [id, tx] : pool_) {
      if (out.size() >= max) break;
      if (used.count(id) != 0) continue;
      if (working->validate(tx).ok) {
        working->apply(tx);
        out.push_back(tx);
        used.insert(id);
        progress = true;
      }
    }
  }
  return out;
}

void Mempool::remove(const std::vector<Hash>& txids) {
  for (const auto& id : txids) {
    if (pool_.erase(id) != 0 && bus_) {
      bus_->publish(observe::TxRemoved{id, pool_.size()});
    }
  }
}

}  // namespace blockchainer
