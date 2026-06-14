#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <vector>

#include "blockchainer/state.hpp"
#include "blockchainer/types.hpp"

namespace blockchainer::observe {
class EventBus;
}  // namespace blockchainer::observe

/// Transaction mempool (ARCHITECTURE §9). Model-agnostic: it leans on the
/// pluggable [StateMachine] to decide which pending txs form a valid, ordered
/// sequence — so account nonce-ordering and UTXO conflict-detection fall out of
/// the same `validate`/`apply` logic the ledger uses, with no model-specific
/// code here.
namespace blockchainer {

struct MempoolConfig {
  std::size_t max_txs = 4096;          ///< capacity (tx count)
  std::size_t max_tx_bytes = 128 * 1024;  ///< reject txs larger than this
};

enum class AddResult {
  kAdded,
  kDuplicate,
  kInvalidSignature,
  kWrongChain,
  kTooLarge,
  kFull,
};

class Mempool {
 public:
  explicit Mempool(MempoolConfig cfg = {}) : cfg_(cfg) {}

  /// Admit a transaction: checks size, chain id, signature, and dedup. Does NOT
  /// check state validity (a future-nonce tx is admitted and becomes selectable
  /// once its predecessors land).
  AddResult add(const Transaction& tx, std::uint64_t chain_id);

  bool contains(const Hash& txid) const { return pool_.count(txid) != 0; }
  std::size_t size() const { return pool_.size(); }

  /// Select up to [max] txs that apply cleanly, in dependency order, against a
  /// clone of [state]. The result is exactly what `Ledger::build_block` will
  /// accept (nonce sequences resolved, UTXO conflicts excluded).
  std::vector<Transaction> select(const StateMachine& state,
                                  std::size_t max) const;

  /// Remove transactions by id (e.g. after a block commits).
  void remove(const std::vector<Hash>& txids);

  /// Attach an event bus (nullable). Publishes TxAdmitted/TxRemoved.
  void set_event_bus(observe::EventBus* bus) { bus_ = bus; }

 private:
  MempoolConfig cfg_;
  // Keyed by txid → deterministic iteration order for selection.
  std::map<Hash, Transaction> pool_;
  observe::EventBus* bus_ = nullptr;
};

}  // namespace blockchainer
