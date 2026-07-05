#include "blockchainer/bft_engine.hpp"

#include "blockchainer/observability/event_bus.hpp"
#include "blockchainer/observability/events.hpp"
#include "blockchainer/observability/metrics.hpp"

namespace blockchainer::consensus {

void BftEngine::set_observability(observe::EventBus* bus,
                                  observe::MetricsRegistry* metrics) {
  bus_ = bus;
  metrics_ = metrics;
}

bool BftEngine::record_evidence(const Evidence& ev) {
  if (!verify_equivocation(ledger_->spec().chain_id, vset_, ev).ok) return false;
  equivocators_.insert(ev.validator_index);
  return true;
}

ValidatorSet BftEngine::slashed_validator_set() const {
  ValidatorSet slashed = vset_;
  for (std::uint32_t idx : equivocators_) slashed = slashed.with_slashed(idx);
  return slashed;
}

BlockError BftEngine::submit(const Block& b, codec::ByteView justification) {
  CommitCertificate cert;
  try {
    cert = CommitCertificate::decode_all(justification);
  } catch (const codec::CodecError& e) {
    return BlockError::failure(std::string("consensus: malformed justification: ") + e.what());
  }
  return finalize(b, cert);
}

Vote BftEngine::precommit(const Block& b, std::uint32_t validator_index,
                          const crypto::KeyPair& bls_key,
                          std::uint32_t round) const {
  return sign_vote(ledger_->spec().chain_id, b.header.height, round, block_id(b),
                   validator_index, bls_key);
}

std::optional<CommitCertificate> BftEngine::try_commit(
    const Block& b, const std::vector<Vote>& votes, std::uint32_t round) const {
  return aggregate_commit(ledger_->spec().chain_id, vset_, b.header.height, round,
                          block_id(b), votes);
}

CommitError BftEngine::check_finalization(const Block& b,
                                          const CommitCertificate& cert) const {
  if (cert.block_id != block_id(b)) {
    return CommitError::failure("certificate block id does not match block");
  }
  if (cert.height != b.header.height) {
    return CommitError::failure("certificate height does not match block");
  }
  // The block must bind to this validator set and carry a valid last commit for
  // its parent (self-certifying history + anti validator-set-swap).
  if (CommitError ce = validate_consensus_data(ledger_->spec().chain_id, vset_, b.header,
                                               b.header.parent_hash);
      !ce.ok) {
    return ce;
  }
  return verify_commit(ledger_->spec().chain_id, vset_, cert);
}

BlockError BftEngine::finalize(const Block& b, const CommitCertificate& cert) {
  if (CommitError ce = check_finalization(b, cert); !ce.ok) {
    if (bus_) bus_->publish(observe::BlockRejected{b.header.height, block_id(b)});
    return BlockError::failure("consensus: " + ce.error);
  }
  // The certificate proves >2/3 agreement; process_block proves the block is a
  // valid state transition and commits it atomically.
  BlockError be = ledger_->process_block(b);

  if (be.ok) {
    last_commit_ = cert;  // becomes the next block's certified last commit
    if (bus_) {
      bus_->publish(observe::BlockFinalized{b.header.height, block_id(b),
                                            b.header.state_root,
                                            b.transactions.size()});
    }
    if (metrics_) {
      metrics_->gauge("blockchainer_chain_height", "current finalized height")
          .set(static_cast<std::int64_t>(b.header.height));
      metrics_->counter("blockchainer_blocks_finalized_total", "blocks finalized").inc();
      metrics_->counter("blockchainer_transactions_total", "transactions applied")
          .inc(b.transactions.size());
    }
  } else if (bus_) {
    bus_->publish(observe::BlockRejected{b.header.height, block_id(b)});
  }
  return be;
}

}  // namespace blockchainer::consensus
