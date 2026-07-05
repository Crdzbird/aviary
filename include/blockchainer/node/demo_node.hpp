#pragma once

#include <cstdint>
#include <memory>
#include <stdexcept>
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

/// Thrown by credit() once genesis is sealed (the first block was produced).
/// A typed exception so the C ABI can surface a stable error code instead of a
/// generic failure — silent loss of a genesis credit is a fund-loss bug.
class GenesisSealedError : public std::runtime_error {
 public:
  GenesisSealedError()
      : std::runtime_error(
            "demo_node: genesis is sealed (a block was produced); fund accounts with transfers") {}
};

class DemoNode {
 public:
  explicit DemoNode(std::uint64_t chain_id);

  // ── Creation ───────────────────────────────────────────────────────────────
  /// Credit an address at genesis. Valid until the first block is produced;
  /// afterwards throws GenesisSealedError (never a silent no-op). Reads do NOT
  /// seal genesis — only produce_block() does.
  void credit(const Hash& address, codec::u128 amount);
  /// Generate a fresh Ed25519 account keypair.
  crypto::KeyPair new_account();
  /// Generate a fresh hybrid (Ed25519 + ML-DSA-65) post-quantum account keypair.
  crypto::KeyPair new_account_hybrid();

  // ── Transactions ────────────────────────────────────────────────────────────
  /// Build, sign, and admit an Ed25519 transfer. [from_secret] is the 64-byte
  /// Ed25519 secret key. Returns the txid.
  Hash submit_transfer(codec::ByteView from_secret, const Hash& to,
                       codec::u128 amount, std::uint64_t nonce);
  /// Build, sign, and admit an Ed25519 transfer carrying a priority-fee [tip]
  /// (a version-2 envelope). Returns the txid.
  Hash submit_transfer_fee(codec::ByteView from_secret, const Hash& to,
                           codec::u128 amount, std::uint64_t nonce, codec::u128 tip);
  /// Build, sign, and admit a hybrid-signed (post-quantum) transfer. [secret] and
  /// [public_key] are the hybrid key blobs (from new_account_hybrid). Returns the txid.
  Hash submit_transfer_hybrid(codec::ByteView secret, codec::ByteView public_key,
                              const Hash& to, codec::u128 amount, std::uint64_t nonce);

  // ── Consensus ────────────────────────────────────────────────────────────────
  /// Produce + finalize a block from the mempool (this node is the validator).
  /// Returns the new height, or throws on failure.
  std::uint64_t produce_block(std::uint64_t timestamp);

  // ── Queries (non-sealing: reading never starts the engine or seals genesis) ──
  std::uint64_t height() const;
  codec::u128 balance(const Hash& address) const;
  std::uint64_t nonce(const Hash& address) const;
  Hash state_root() const;
  std::size_t mempool_size() const { return mempool_.size(); }

  // ── Observability / streaming ────────────────────────────────────────────────
  /// Drain collected chain events as a JSON array, clearing the buffer.
  std::string drain_events_json();
  /// Prometheus-format metrics snapshot.
  std::string metrics_text();

  // ── Verification (light client) ──────────────────────────────────────────────
  /// State-inclusion proof for [address], verifiable against state_root().
  StateProof prove_account(const Hash& address) const;

 private:
  void ensure_started();
  /// The current account state for READS: the engine's once started, otherwise
  /// the (still-mutable) genesis state. Never starts the engine.
  const AccountStateMachine& read_state() const;

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
