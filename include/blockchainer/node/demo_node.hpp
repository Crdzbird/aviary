#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/mempool.hpp"
#include "blockchainer/observability/event_bus.hpp"
#include "blockchainer/observability/metrics.hpp"
#include "blockchainer/proofs.hpp"

/// A self-contained, single-validator account-model node wiring together every
/// blockchainer layer — ledger + BFT consensus + mempool + observability +
/// verification — behind one simple object. This is the façade the C ABI (and
/// thus the Flutter demo) drives. Not for production: one validator, in-memory.
namespace blockchainer::node {

class DemoNode {
 public:
  explicit DemoNode(std::uint64_t chain_id);

  // ── Creation ───────────────────────────────────────────────────────────────
  /// Credit an address at genesis (only meaningful before the first block).
  void credit(const Hash& address, codec::u128 amount);
  /// Generate a fresh Ed25519 account keypair.
  crypto::KeyPair new_account();

  // ── Transactions ────────────────────────────────────────────────────────────
  /// Build, sign, and admit a transfer to the mempool. [from_secret] is the
  /// 64-byte Ed25519 secret key. Returns the txid.
  Hash submit_transfer(codec::ByteView from_secret, const Hash& to,
                       codec::u128 amount, std::uint64_t nonce);

  // ── Consensus ────────────────────────────────────────────────────────────────
  /// Produce + finalize a block from the mempool (this node is the validator).
  /// Returns the new height, or throws on failure.
  std::uint64_t produce_block(std::uint64_t timestamp);

  // ── Queries (start the engine lazily, so credit() can run first) ─────────────
  std::uint64_t height();
  codec::u128 balance(const Hash& address);
  std::uint64_t nonce(const Hash& address);
  Hash state_root();
  std::size_t mempool_size() const { return mempool_.size(); }

  // ── Observability / streaming ────────────────────────────────────────────────
  /// Drain collected chain events as a JSON array, clearing the buffer.
  std::string drain_events_json();
  /// Prometheus-format metrics snapshot.
  std::string metrics_text();

  // ── Verification (light client) ──────────────────────────────────────────────
  /// State-inclusion proof for [address], verifiable against state_root().
  StateProof prove_account(const Hash& address);

 private:
  void ensure_started();
  AccountStateMachine& account_state();

  std::uint64_t chain_id_;
  crypto::KeyPair validator_;  // BLS — the sole validator
  std::unique_ptr<AccountStateMachine> genesis_;  // mutable until first start
  std::unique_ptr<consensus::BftEngine> engine_;  // built on first use
  Mempool mempool_;
  observe::EventBus bus_;
  observe::MetricsRegistry metrics_;
  std::vector<std::string> events_;  // JSON fragments collected from the bus
};

}  // namespace blockchainer::node
