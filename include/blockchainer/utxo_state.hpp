#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/merkle.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/state.hpp"

namespace blockchainer {

/// UTXO-model state machine (ARCHITECTURE §5). State is the set of unspent
/// outputs, committed as a Merkle root over canonically encoded, outpoint-sorted
/// UTXO leaves. Replay protection is intrinsic: an outpoint is spendable once.
///
/// M1 simplification: a transaction has a single signer ([Transaction::author]),
/// so every input it spends must be owned by that author.
///
/// **Fees.** A transaction may spend more than it creates; the surplus
/// `sum(inputs) − sum(outputs)` is the fee (burned, no fee output). A chain sets
/// a minimum via [flat_fee]; 0 (the default) requires exact value conservation,
/// byte-identical to a chain with no fee concept.
///
/// **Commitment performance (copy-on-write).** Like the account model, the
/// committed UTXO set and its cached Merkle tree live in an immutable, shared
/// `Snapshot`; a block's spends/creations accumulate in a small `overlay_` (a
/// tombstone marks a spent outpoint). So `clone()` — done once per block to
/// apply it atomically — shares the snapshot and copies only the overlay
/// (O(changed)), not the whole UTXO set. The committed root is byte-identical to
/// `merkle::compute_root` over the sorted leaves.
///
/// Body layout (canonical): `list<Outpoint> inputs ‖ list<Output> outputs`,
/// where `Outpoint = fixed32 txid ‖ u32 index` and `Output = fixed32 owner ‖ u128 amount`.
class UtxoStateMachine : public StateMachine {
 public:
  struct Outpoint {
    Hash txid{};
    std::uint32_t index = 0;
    bool operator<(const Outpoint& o) const {
      return txid != o.txid ? txid < o.txid : index < o.index;
    }
    bool operator==(const Outpoint& o) const {
      return txid == o.txid && index == o.index;
    }
  };
  struct Output {
    Hash owner{};
    codec::u128 amount = 0;
  };
  struct Body {
    std::vector<Outpoint> inputs;
    std::vector<Output> outputs;
  };

  /// [flat_fee] is the minimum fee (surplus of inputs over outputs) a spend must
  /// pay; 0 (the default) requires exact value conservation.
  explicit UtxoStateMachine(codec::u128 flat_fee = 0);

  codec::u128 flat_fee() const { return flat_fee_; }

  /// Genesis helper: create an unspent output and return its (synthetic)
  /// outpoint, so tests/genesis can later spend it.
  Outpoint add_genesis_utxo(const Output& output);

  bool has(const Outpoint& op) const { return lookup(op) != nullptr; }
  Output get(const Outpoint& op) const;
  std::size_t utxo_count() const;

  StateModel model() const override { return StateModel::kUtxo; }
  Hash state_root() const override;
  ValidationResult validate(const Transaction& tx) const override;
  void apply(const Transaction& tx) override;
  std::unique_ptr<StateMachine> clone() const override;

  static Body decode_body(const Bytes& body);
  static Bytes encode_body(const Body& b);

  /// State-inclusion proof that [op] is an unspent output under state_root().
  StateProof prove_outpoint(const Outpoint& op) const;

 private:
  // Immutable, shareable committed baseline: the UTXO set (outpoint-sorted), the
  // same outpoints in leaf order, and the cached Merkle tree over their leaves.
  struct Snapshot {
    std::map<Outpoint, Output> utxos;
    std::vector<Outpoint> sorted_ops;
    merkle::MerkleIndex tree;
  };

  // Committed baseline (shared) plus uncommitted changes: a present Output is a
  // created/updated UTXO, std::nullopt is a tombstone for a spent one. Mutable so
  // the const commitment path can materialise a fresh snapshot (logical
  // constness). Single-threaded, like the rest of the node.
  mutable std::shared_ptr<const Snapshot> base_;
  mutable std::map<Outpoint, std::optional<Output>> overlay_;
  codec::u128 flat_fee_ = 0;
  std::uint64_t genesis_counter_ = 0;

  const Output* lookup(const Outpoint& op) const;  // overlay-then-base, nullptr if absent
  void seal() const;                               // fold overlay → new base_
  std::map<Outpoint, Output> merged() const;       // base with overlay applied
};

}  // namespace blockchainer
