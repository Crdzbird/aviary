#pragma once

#include <memory>
#include <string>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// The pluggable state machine (ARCHITECTURE §5). The block-processing pipeline
/// is written once against this interface; account and UTXO implementations slot
/// in. All methods are deterministic — no I/O, no clock, no randomness.
namespace aviary {

struct ValidationResult {
  bool ok = false;
  std::string error;

  static ValidationResult success() { return {true, {}}; }
  static ValidationResult failure(std::string e) { return {false, std::move(e)}; }
};

class StateMachine {
 public:
  virtual ~StateMachine() = default;

  /// Which ledger model this is.
  virtual StateModel model() const = 0;

  /// Deterministic, canonical commitment to the current state.
  virtual Hash state_root() const = 0;

  /// Stateless + stateful checks against current state. Pure; does not mutate.
  /// Signature verification is done by the pipeline, not here.
  virtual ValidationResult validate(const Transaction& tx) const = 0;

  /// Applies a previously-validated tx. Precondition: validate(tx).ok.
  virtual void apply(const Transaction& tx) = 0;

  /// The fee a valid [tx] pays against the CURRENT (pre-apply) state — 0 for a
  /// model with no fee. Used to build execution receipts; call before apply().
  virtual codec::u128 tx_fee(const Transaction& /*tx*/) const { return 0; }

  /// Deep copy, so the pipeline can apply a block atomically on a working copy
  /// and commit only on success.
  virtual std::unique_ptr<StateMachine> clone() const = 0;

  /// Serialize the full committed state to a self-describing byte blob (carries
  /// its own flat fee), so a chain can be snapshotted and reloaded. Round-trips
  /// with the model's `deserialize` to a byte-identical state root.
  virtual Bytes serialize() const = 0;
};

/// Reconstruct a state machine of [model] from bytes produced by
/// StateMachine::serialize(). Throws on a malformed blob.
std::unique_ptr<StateMachine> deserialize_state(StateModel model, codec::ByteView bytes);

}  // namespace aviary
