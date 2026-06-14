#pragma once

#include <memory>
#include <string>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// The pluggable state machine (ARCHITECTURE §5). The block-processing pipeline
/// is written once against this interface; account and UTXO implementations slot
/// in. All methods are deterministic — no I/O, no clock, no randomness.
namespace blockchainer {

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

  /// Deep copy, so the pipeline can apply a block atomically on a working copy
  /// and commit only on success.
  virtual std::unique_ptr<StateMachine> clone() const = 0;
};

}  // namespace blockchainer
