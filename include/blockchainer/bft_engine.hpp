#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "blockchainer/consensus.hpp"
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

class BftEngine {
 public:
  BftEngine(ValidatorSet vset, std::unique_ptr<Ledger> ledger)
      : vset_(std::move(vset)), ledger_(std::move(ledger)) {}

  const Ledger& ledger() const { return *ledger_; }
  const ValidatorSet& validators() const { return vset_; }
  std::uint64_t height() const { return ledger_->tip().height; }
  const BlockHeader& tip() const { return ledger_->tip(); }

  /// The id validators vote on for [b].
  static Hash block_id(const Block& b) { return b.header.hash(); }

  /// Proposer step: assemble the next block from [txs].
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
    return ledger_->build_block(std::move(txs), timestamp);
  }

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

 private:
  ValidatorSet vset_;
  std::unique_ptr<Ledger> ledger_;
  observe::EventBus* bus_ = nullptr;
  observe::MetricsRegistry* metrics_ = nullptr;
};

}  // namespace blockchainer::consensus
