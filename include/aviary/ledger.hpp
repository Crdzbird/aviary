#pragma once

#include <stdexcept>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aviary/state.hpp"
#include "aviary/subchain.hpp"
#include "aviary/types.hpp"

namespace aviary {

/// Genesis parameters that fully parameterize a chain (ARCHITECTURE section 0).
struct ChainSpec {
  std::uint64_t chain_id = 0;
  StateModel state_model = StateModel::kAccount;
  /// Transaction signature schemes this chain accepts (SigAlgo values). Empty =
  /// accept every scheme the node knows. A chain can require post-quantum
  /// signatures by setting this to {kHybridEd25519MlDsa}.
  std::vector<std::uint8_t> accepted_sig_algos{};

  /// Flat protocol fee burned from the sender on every transfer (account model).
  /// 0 = feeless; the state transition is then byte-identical to a chain with no
  /// fee concept. A non-zero fee gives spam a real, per-tx economic cost.
  codec::u128 flat_fee = 0;

  /// Consensus-level block bounds enforced on EVERY block (self-produced and
  /// received), so a peer cannot force O(huge) work with one oversized block.
  /// 0 = unlimited. These are the hard protocol ceilings; the mempool's own
  /// limits (which shape self-produced blocks) should sit at or below them.
  std::uint32_t max_block_txs = 0;
  std::uint64_t max_block_bytes = 0;  ///< sum of encoded tx sizes

  /// When true, produced blocks are version-2 and commit an execution
  /// `receipt_root` (one receipt per tx). Default false -> version-1 blocks, no
  /// receipts, byte-identical to before.
  bool produce_receipts = false;

  /// Canonical `SubchainPolicy` bytes (docs/design/SUBCHAINS.md) when this
  /// chain is a PARENT. Empty = anchors nothing; every version-3 header is then
  /// refused. Non-empty = every produced block is version 3 and commits the
  /// child-tip registry; a lower-version header is refused.
  Bytes subchain_policy{};

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
/// applies blocks atomically (ARCHITECTURE section 6), and can produce the next block.
/// Consensus is out of scope here (M2) - this is the deterministic state layer
/// every consensus engine drives.
class Ledger {
 public:
  Ledger(ChainSpec spec, std::unique_ptr<StateMachine> genesis_state,
         std::uint64_t genesis_timestamp = 0);

  const ChainSpec& spec() const { return spec_; }
  const BlockHeader& tip() const { return tip_; }
  const StateMachine& state() const { return *state_; }

  /// Execution receipts of the most recently committed block (empty if that
  /// block was version 1 / carried no receipts). Lets a prover build a
  /// receipt-inclusion proof against the committed receipt_root.
  const std::vector<Receipt>& last_receipts() const { return last_receipts_; }

  /// Single-node producer: assemble the next block from [txs] (and, on a parent
  /// chain, the subchain [anchors]). Does NOT mutate the ledger; the returned
  /// block commits the resulting tx/state (and anchor/subchain) roots. Throws
  /// std::runtime_error (with the reason) if any tx or anchor is invalid.
  Block build_block(std::vector<Transaction> txs, std::uint64_t timestamp,
                    std::vector<Anchor> anchors = {});

  /// The subchain registry (child tips) when this chain is a parent, else null.
  const SubchainRegistry* subchains() const { return registry_ ? &*registry_ : nullptr; }

  /// Validate + apply [block] atomically. On success the ledger advances; on
  /// failure it is left completely unchanged.
  BlockError process_block(const Block& block);

  /// Dry run of exactly the checks process_block applies (header, limits,
  /// roots, every tx, every subchain anchor) with NO state change. A consensus
  /// driver uses it before voting on a proposal, so a block the ledger would
  /// refuse can never be certified (docs/design/SUBCHAINS.md section 6).
  BlockError validate_block(const Block& block) const;

  /// Serialize the chain tip + full committed state to a self-describing blob a
  /// node can persist and reload (foundation for storage + fast sync).
  Bytes snapshot() const;

  /// Rebuild a ledger from a [snapshot] under [spec]. The restored ledger is at
  /// the snapshotted height and accepts the next block. Throws on a bad blob.
  static std::unique_ptr<Ledger> restore(ChainSpec spec, codec::ByteView snapshot);

 private:
  BlockError check_header(const BlockHeader& h) const;
  // Shared body of validate_block / process_block: runs the full pipeline on a
  // working copy and hands back the post-state for the caller to commit or drop.
  struct Worked {
    std::unique_ptr<StateMachine> state;
    std::vector<Receipt> receipts;
    std::optional<SubchainRegistry> registry;
  };
  BlockError run_block(const Block& block, Worked& out) const;

  ChainSpec spec_;
  std::unique_ptr<StateMachine> state_;
  BlockHeader tip_;
  std::vector<Receipt> last_receipts_;
  std::optional<SubchainRegistry> registry_;  // parent chains only
};

}  // namespace aviary
