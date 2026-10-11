#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "aviary/state.hpp"
#include "aviary/types.hpp"

namespace aviary::observe {
class EventBus;
}  // namespace aviary::observe

namespace aviary::consensus {
class CredentialGate;
}  // namespace aviary::consensus

/// Transaction mempool (ARCHITECTURE §9). Model-agnostic: it leans on the
/// pluggable [StateMachine] to decide which pending txs form a valid, ordered
/// sequence — so account nonce-ordering and UTXO conflict-detection fall out of
/// the same `validate`/`apply` logic the ledger uses, with no model-specific
/// code here.
namespace aviary {

struct MempoolConfig {
  std::size_t max_txs = 4096;             ///< capacity (tx count)
  std::size_t max_tx_bytes = 128 * 1024;  ///< reject txs larger than this
  /// Max pending txs from a single sender (address = BLAKE3(author)). Bounds the
  /// classic griefing lever where one account floods the pool and starves
  /// others. 0 disables the per-account cap. Only applies to txs with a single
  /// author (the account model); authorless/multi-author txs are not capped.
  std::size_t max_per_account = 64;
  /// Max total encoded bytes a produced block may contain. select() stops once
  /// the next tx would exceed it — bounds block size independent of the tx count.
  std::size_t max_block_bytes = 1024 * 1024;
};

enum class AddResult {
  kAdded,
  kDuplicate,
  kInvalidSignature,
  kWrongChain,
  kTooLarge,
  kFull,
  kAccountLimit,  ///< sender already has max_per_account pending txs
  kNotAdmitted,   ///< a membership gate is attached and the presentation did not admit
};

class Mempool {
 public:
  explicit Mempool(MempoolConfig cfg = {}) : cfg_(cfg) {}

  /// Admit a transaction: checks size, chain id, signature, and dedup. Does NOT
  /// check state validity (a future-nonce tx is admitted and becomes selectable
  /// once its predecessors land). This is the TRUSTED path — it is not membership-
  /// gated even when a gate is attached (use it for node-internal submissions).
  AddResult add(const Transaction& tx, std::uint64_t chain_id);

  /// Submission from an (anonymous) member. When a credential gate is attached
  /// (see [set_credential_gate]), the [presentation] must admit under scope
  /// "submit" bound to ([epoch], [challenge], [now]) — otherwise kNotAdmitted and
  /// the tx is not added. Then the normal [add] checks run. With no gate attached
  /// this behaves exactly like `add(tx, chain_id)` (the presentation is ignored),
  /// so an ungated (permissionless) chain is unaffected. The gate is a *local*
  /// admission policy — [now] is caller-supplied, so the mempool stays clock-free.
  AddResult add(const Transaction& tx, std::uint64_t chain_id, std::uint32_t epoch,
                crypto::ByteView challenge, crypto::ByteView presentation, std::uint64_t now);

  bool contains(const Hash& txid) const { return pool_.count(txid) != 0; }
  std::size_t size() const { return pool_.size(); }

  /// Select up to [max] txs that apply cleanly, in dependency order, against a
  /// clone of [state]. The result is exactly what `Ledger::build_block` will
  /// accept (nonce sequences resolved, UTXO conflicts excluded).
  std::vector<Transaction> select(const StateMachine& state,
                                  std::size_t max) const;

  /// Remove transactions by id (e.g. after a block commits).
  void remove(const std::vector<Hash>& txids);
  /// The highest nonce among the account model transfers pending from the
  /// account at [sender], or nullopt when none is pending.
  std::optional<std::uint64_t> highest_pending_nonce(const Hash& sender) const;

  /// Attach an event bus (nullable). Publishes TxAdmitted/TxRemoved.
  void set_event_bus(observe::EventBus* bus) { bus_ = bus; }

  /// Attach an anonymous-membership admission gate (nullable). When set, the
  /// gated [add] overload requires a valid "submit"-scope presentation before a
  /// tx is admitted (docs/design/CREDENTIAL_GATE.md, Option A). The gate is only
  /// read (its admit() is const); ownership stays with the caller.
  void set_credential_gate(const consensus::CredentialGate* gate) { gate_ = gate; }

 private:
  MempoolConfig cfg_;
  // Keyed by txid → deterministic iteration order for selection.
  std::map<Hash, Transaction> pool_;
  // Pending-tx count per sender address (BLAKE3(author)), for the per-account
  // cap. Only tracks txs with a non-empty author.
  std::map<Hash, std::size_t> per_account_;
  observe::EventBus* bus_ = nullptr;
  const consensus::CredentialGate* gate_ = nullptr;
};

}  // namespace aviary
