#pragma once

#include <functional>
#include <memory>
#include <string>

#include "blockchainer/consensus_engine.hpp"

/// Components implemented OUTSIDE C++ (Dart, Go, JS, Swift, JVM …) joining the
/// mixture. The consensus-critical pipeline stays in C++ — a foreign engine's
/// propose() builds via the Ledger and its submit() applies via the Ledger, so
/// atomicity and canonical encoding are never delegated. What the foreign code
/// supplies is the part that genuinely varies per engine: the JUSTIFICATION
/// policy — how a block this node proposes is justified, and whether a
/// justification presented with a block is valid. A foreign proposer policy is
/// the pure leader-election function.
///
/// CONTRACT for foreign code (the registry cannot check it): the callbacks are
/// invoked synchronously inside consensus and MUST be deterministic — no clock,
/// no randomness, no map-iteration order — and must not re-enter the chain.
namespace blockchainer::node {

using JustifyFn = std::function<Bytes(const Block&)>;
using VerifyFn = std::function<bool(const Block&, crypto::ByteView justification)>;
using ForeignProposerFn = std::function<std::uint32_t(std::uint64_t height, std::uint32_t validators)>;

class ForeignEngine : public consensus::ConsensusEngine {
 public:
  ForeignEngine(std::unique_ptr<Ledger> ledger, JustifyFn justify, VerifyFn verify);

  const Ledger& ledger() const override { return *ledger_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) override;
  Block propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                         std::vector<Anchor> anchors) override;
  /// verify(block, justification) via the foreign policy, then apply atomically.
  BlockError submit(const Block& block, codec::ByteView justification) override;
  /// The foreign justification for a block this node proposed (its "seal").
  Bytes justify(const Block& block) const { return justify_(block); }

 private:
  std::unique_ptr<Ledger> ledger_;
  JustifyFn justify_;
  VerifyFn verify_;
};

/// Register a foreign engine under [id] (chain identity — never rename).
/// [grind_proof] declares whether its leader election is unpredictable.
void register_foreign_engine(const std::string& id, JustifyFn justify, VerifyFn verify,
                             bool grind_proof);
/// Register a foreign proposer policy under [id].
void register_foreign_proposer(const std::string& id, ForeignProposerFn fn);

}  // namespace blockchainer::node
