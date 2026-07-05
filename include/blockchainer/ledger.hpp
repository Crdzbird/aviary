#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "blockchainer/state.hpp"
#include "blockchainer/types.hpp"

namespace blockchainer {

/// Genesis parameters that fully parameterize a chain (ARCHITECTURE §0).
struct ChainSpec {
  std::uint64_t chain_id = 0;
  StateModel state_model = StateModel::kAccount;
  /// Transaction signature schemes this chain accepts (SigAlgo values). Empty =
  /// accept every scheme the node knows. A chain can require post-quantum
  /// signatures by setting this to {kHybridEd25519MlDsa}.
  std::vector<std::uint8_t> accepted_sig_algos{};

  /// Whether [algo] is permitted on this chain.
  bool accepts(SigAlgo algo) const {
    if (accepted_sig_algos.empty()) return true;
    for (std::uint8_t a : accepted_sig_algos) {
      if (a == static_cast<std::uint8_t>(algo)) return true;
    }
    return false;
  }
};

struct BlockError {
  bool ok = true;
  std::string error;
  static BlockError success() { return {true, {}}; }
  static BlockError failure(std::string e) { return {false, std::move(e)}; }
};

/// A single-node ledger: holds the committed state and chain tip, validates and
/// applies blocks atomically (ARCHITECTURE §6), and can produce the next block.
/// Consensus is out of scope here (M2) — this is the deterministic state layer
/// every consensus engine drives.
class Ledger {
 public:
  Ledger(ChainSpec spec, std::unique_ptr<StateMachine> genesis_state,
         std::uint64_t genesis_timestamp = 0);

  const ChainSpec& spec() const { return spec_; }
  const BlockHeader& tip() const { return tip_; }
  const StateMachine& state() const { return *state_; }

  /// Single-node producer: assemble the next block from [txs]. Does NOT mutate
  /// the ledger; the returned block commits the resulting tx/state roots. Throws
  /// std::runtime_error (with the reason) if any tx is invalid.
  Block build_block(std::vector<Transaction> txs, std::uint64_t timestamp);

  /// Validate + apply [block] atomically. On success the ledger advances; on
  /// failure it is left completely unchanged.
  BlockError process_block(const Block& block);

 private:
  BlockError check_header(const BlockHeader& h) const;

  ChainSpec spec_;
  std::unique_ptr<StateMachine> state_;
  BlockHeader tip_;
};

}  // namespace blockchainer
