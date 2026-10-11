#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/mempool.hpp"
#include "aviary/observability/event_bus.hpp"
#include "aviary/observability/event_log.hpp"
#include "aviary/observability/metrics.hpp"
#include "aviary/proofs.hpp"

/// A self-contained, single-validator account-model node wiring together every
/// aviary layer — ledger + BFT consensus + mempool + observability +
/// verification — behind one simple object. This is the façade the C ABI (and
/// thus the Flutter demo) drives. Not for production: one validator, in-memory.
namespace aviary::node {

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

  /// Admit an already signed transaction (for example from
  /// custody::sign_transfer). Returns the txid; throws when the mempool
  /// refuses it.
  Hash submit(const Transaction& tx);
  /// submit() of an encoded transaction.
  Hash submit_encoded(codec::ByteView tx_bytes);
  // ── Consensus ────────────────────────────────────────────────────────────────
  /// Produce + finalize a block from the mempool (this node is the validator).
  /// Returns the new height, or throws on failure.
  std::uint64_t produce_block(std::uint64_t timestamp);

  // ── Queries (non-sealing: reading never starts the engine or seals genesis) ──
  std::uint64_t chain_id() const { return chain_id_; }
  std::uint64_t height() const;
  codec::u128 balance(const Hash& address) const;
  std::uint64_t nonce(const Hash& address) const;
  /// The nonce the next transfer from [address] must carry: the account nonce
  /// advanced past every transfer from it already pending in the mempool.
  std::uint64_t next_nonce(const Hash& address) const;
  Hash state_root() const;
  std::size_t mempool_size() const { return mempool_.size(); }

  // ── Observability / streaming ────────────────────────────────────────────────
  /// Drain collected chain events as a JSON array, clearing the buffer.
  std::string drain_events_json();
  /// Receives each chain event as one JSON object, synchronously, as it is
  /// published. While a hook is set events are delivered to it instead of
  /// being buffered for drain_events_json(); pass an empty function to go
  /// back to buffering.
  using EventHook = std::function<void(const std::string& json)>;
  void set_event_hook(EventHook hook);
  /// Prometheus-format metrics snapshot.
  std::string metrics_text();

  // Persistence
  /// The whole chain as one blob: chain id, genesis credits, and every
  /// finalized block with its commit certificate. The mempool is not included.
  Bytes snapshot() const;
  /// Rebuilds a node from snapshot(). A current snapshot carries the ledger
  /// state, so the node resumes from the tip in constant time; the block
  /// archive is kept for block_at. [replay] forces the slow path instead:
  /// recreate the genesis credits and replay every block through consensus,
  /// verifying each certificate. A snapshot from before the ledger state was
  /// included always replays. Throws std::runtime_error on a malformed blob
  /// or a block that fails.
  static std::unique_ptr<DemoNode> restore(codec::ByteView bytes, bool replay = false);
  /// The finalized block at [height] (1 based). Throws std::out_of_range if
  /// absent.
  const Block& block_at(std::uint64_t height) const { return archive_.by_height(height).block; }
  /// Finalized blocks held in the archive.
  std::size_t archived_blocks() const { return archive_.size(); }
  // ── Verification (light client) ──────────────────────────────────────────────
  /// State-inclusion proof for [address], verifiable against state_root().
  StateProof prove_account(const Hash& address) const;

 private:
  void ensure_started();
  void start_engine(std::unique_ptr<Ledger> ledger);
  /// The current account state for READS: the engine's once started, otherwise
  /// the (still-mutable) genesis state. Never starts the engine.
  const AccountStateMachine& read_state() const;

  std::uint64_t chain_id_;
  crypto::KeyPair validator_;  // BLS — the sole validator
  std::unique_ptr<AccountStateMachine> genesis_;  // mutable until first start
  std::unique_ptr<consensus::BftEngine> engine_;  // built on first use
  Mempool mempool_;
  BlockStore archive_;  // every finalized block with its certificate
  std::vector<std::pair<Hash, codec::u128>> credits_;  // genesis credits, in order
  observe::EventBus bus_;
  observe::MetricsRegistry metrics_;
  observe::JsonEventLog log_{bus_};
};

}  // namespace aviary::node
