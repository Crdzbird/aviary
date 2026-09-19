#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/types.hpp"

/// The pluggable consensus engine interface (ARCHITECTURE §7). Consensus drives
/// the deterministic [Ledger] state layer; the *family* (BFT, PoW, PoS) decides
/// which blocks are canonical and how a block is justified. This interface is
/// the family-agnostic surface: query the chain, propose a candidate block, and
/// submit a fully-justified block for commit.
///
/// The **justification** is where families differ, so it is an opaque byte
/// string here: a BFT engine's justification is an encoded commit certificate; a
/// PoW engine's is a proof-of-work solution; a PoS engine's is its finality
/// proof. Each engine defines and verifies its own encoding — so the pipeline
/// above (sync, RPC, the node façade) can be written once against this seam while
/// a new engine slots in underneath. Engines keep their family-specific API
/// (e.g. `BftEngine::precommit`/`try_commit`) as concrete extras.
namespace blockchainer::consensus {

class ConsensusEngine {
 public:
  virtual ~ConsensusEngine() = default;

  virtual const Ledger& ledger() const = 0;
  virtual std::uint64_t height() const = 0;
  virtual const BlockHeader& tip() const = 0;

  /// Proposer / miner step: assemble the next candidate block from [txs].
  virtual Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) = 0;

  /// The same, on a PARENT chain also anchoring [anchors] (its children's
  /// finalized headers — docs/design/SUBCHAINS.md). Every built-in engine
  /// implements it; a custom engine that does not override it can only
  /// propose anchor-free blocks (throws on non-empty anchors, never drops them).
  virtual Block propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                                 std::vector<Anchor> anchors) {
    if (!anchors.empty()) throw std::runtime_error("this consensus engine does not anchor subchains");
    return propose(std::move(txs), timestamp);
  }

  /// Verify [block]'s [justification] and, if valid, apply it atomically —
  /// advancing the chain on success, leaving it unchanged on failure. The
  /// justification encoding is engine-specific.
  virtual BlockError submit(const Block& block, codec::ByteView justification) = 0;
};

}  // namespace blockchainer::consensus
