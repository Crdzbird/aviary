#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/consensus_engine.hpp"
#include "blockchainer/ledger.hpp"

namespace blockchainer::observe {
class EventBus;
class MetricsRegistry;
}  // namespace blockchainer::observe

/// Tendermint-style BFT engine (ARCHITECTURE §7): deterministic instant finality
/// over a known validator set, driving the deterministic [Ledger] state layer.
///
/// M2 scope is the safety-critical core — propose, precommit, aggregate a >2/3
/// commit certificate, and finalize (verify the certificate, then apply the
/// block). The full asynchronous round state machine (timeouts, prevotes,
/// locking, proposer rotation) builds on this and is future work; the safety
/// guarantee (no two conflicting commits) is already established by quorum
/// intersection in the consensus layer.
namespace blockchainer::consensus {

class BftEngine : public ConsensusEngine {
 public:
  BftEngine(ValidatorSet vset, std::unique_ptr<Ledger> ledger)
      : vset_(std::move(vset)), ledger_(std::move(ledger)) {}

  const Ledger& ledger() const override { return *ledger_; }
  const ValidatorSet& validators() const { return vset_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }

  /// The id validators vote on for [b].
  static Hash block_id(const Block& b) { return b.header.hash(); }

  /// Proposer step: assemble the next block from [txs], stamping its
  /// consensus_data with the validator-set hash and the last commit (the
  /// certificate that finalized the parent) — so the produced chain is
  /// self-certifying and bound to this validator set.
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) override {
    return propose(std::move(txs), timestamp, {});
  }
  /// A parent chain's proposal: the same block, also anchoring [anchors]
  /// (docs/design/SUBCHAINS.md). Throws if an anchor is invalid.
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp, std::vector<Anchor> anchors) {
    Block b = ledger_->build_block(std::move(txs), timestamp, std::move(anchors));
    b.header.consensus_data = ConsensusData{vset_.hash(), last_commit_}.encode();
    return b;
  }

  /// ConsensusEngine seam: the justification is an encoded CommitCertificate.
  /// Decodes it and finalizes the block (verify certificate → apply).
  BlockError submit(const Block& b, codec::ByteView justification) override;

  /// A validator precommits a proposed block.
  Vote precommit(const Block& b, std::uint32_t validator_index,
                 const crypto::KeyPair& bls_key, std::uint32_t round = 0) const;

  /// Try to form a commit certificate from collected precommits.
  std::optional<CommitCertificate> try_commit(
      const Block& b, const std::vector<Vote>& votes, std::uint32_t round = 0) const;

  /// Verify that [cert] finalizes [b], without applying it (light-client path).
  CommitError check_finalization(const Block& b,
                                 const CommitCertificate& cert) const;

  /// Verify the certificate, then validate + apply the block. Advances the chain
  /// on success; leaves it unchanged on any failure.
  BlockError finalize(const Block& b, const CommitCertificate& cert);

  /// Attach observability sinks (both nullable). The engine then publishes
  /// BlockFinalized/BlockRejected and updates chain metrics. Purely a
  /// side-channel — never affects consensus or state.
  void set_observability(observe::EventBus* bus, observe::MetricsRegistry* metrics);

  // ── Equivocation evidence pool (S4) ────────────────────────────────────────

  /// Verify and record gossiped double-signing evidence. Returns true iff the
  /// evidence is a valid proof against this chain's validator set (and thus the
  /// offender is now recorded). Idempotent — re-submitting the same offender is
  /// a no-op that still returns true.
  bool record_evidence(const Evidence& ev);

  /// Validator indices proven to have equivocated (recorded via record_evidence).
  const std::set<std::uint32_t>& equivocators() const { return equivocators_; }

  /// The current validator set with every recorded equivocator's power zeroed.
  /// This is the set a governance/validator-set-change action would install; the
  /// engine does not rotate to it automatically (that needs epoch machinery).
  ValidatorSet slashed_validator_set() const;

 private:
  ValidatorSet vset_;
  std::unique_ptr<Ledger> ledger_;
  // The certificate that finalized the current tip — embedded as the next
  // block's last commit. Absent until the first block finalizes.
  std::optional<CommitCertificate> last_commit_;
  std::set<std::uint32_t> equivocators_;  // proven double-signers
  observe::EventBus* bus_ = nullptr;
  observe::MetricsRegistry* metrics_ = nullptr;
};

}  // namespace blockchainer::consensus
