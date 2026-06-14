#pragma once

#include <cstdint>
#include <map>

#include "blockchainer/codec.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/state.hpp"

namespace blockchainer {

/// UTXO-model state machine (ARCHITECTURE §5). State is the set of unspent
/// outputs, committed as a Merkle root over canonically encoded, outpoint-sorted
/// UTXO leaves. Replay protection is intrinsic: an outpoint is spendable once.
///
/// M1 simplification: a transaction has a single signer ([Transaction::author]),
/// so every input it spends must be owned by that author, and value is conserved
/// exactly (sum inputs == sum outputs; fees arrive with the fee model later).
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

  /// Genesis helper: create an unspent output and return its (synthetic)
  /// outpoint, so tests/genesis can later spend it.
  Outpoint add_genesis_utxo(const Output& output);

  bool has(const Outpoint& op) const { return utxos_.count(op) != 0; }
  Output get(const Outpoint& op) const;
  std::size_t utxo_count() const { return utxos_.size(); }

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
  std::map<Outpoint, Output> utxos_;
  std::uint64_t genesis_counter_ = 0;
};

}  // namespace blockchainer
