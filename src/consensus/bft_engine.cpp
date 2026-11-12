#include <optional>
#include "aviary/bft_engine.hpp"

#include <stdexcept>

#include "aviary/observability/event_bus.hpp"
#include "aviary/observability/events.hpp"
#include "aviary/observability/metrics.hpp"

namespace aviary::consensus {

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

Bytes BftEngine::consensus_state() const {
  codec::CodecWriter w;
  const Bytes current = vset_.encode();
  const Bytes certifying = prev_vset_.encode();
  w.write_bytes(codec::ByteView(current.data(), current.size()));
  w.write_bytes(codec::ByteView(certifying.data(), certifying.size()));
  w.write_bool(last_commit_.has_value());
  if (last_commit_) {
    const Bytes cert = last_commit_->encode();
    w.write_bytes(codec::ByteView(cert.data(), cert.size()));
  }
  w.write_bool(pending_.has_value());
  if (pending_) {
    const Bytes next = pending_->encode();
    w.write_bytes(codec::ByteView(next.data(), next.size()));
  }
  const std::vector<std::uint32_t> offenders(equivocators_.begin(), equivocators_.end());
  w.write_list(offenders, [](codec::CodecWriter& ww, std::uint32_t i) { ww.write_u32(i); });
  return w.take();
}

void BftEngine::restore_consensus_state(codec::ByteView state) {
  codec::CodecReader r(state);
  const Bytes current_bytes = r.read_bytes();
  const Bytes certifying_bytes = r.read_bytes();
  std::optional<CommitCertificate> cert;
  if (r.read_bool()) {
    const Bytes c = r.read_bytes();
    cert = CommitCertificate::decode_all(codec::ByteView(c.data(), c.size()));
  }
  std::optional<ValidatorSet> pending;
  if (r.read_bool()) {
    const Bytes p = r.read_bytes();
    pending = ValidatorSet::decode_all(codec::ByteView(p.data(), p.size()));
  }
  const std::vector<std::uint32_t> offenders =
      r.read_list<std::uint32_t>([](codec::CodecReader& rr) { return rr.read_u32(); });
  r.assert_fully_consumed();

  ValidatorSet current = ValidatorSet::decode_all(codec::ByteView(current_bytes.data(), current_bytes.size()));
  ValidatorSet certifying =
      ValidatorSet::decode_all(codec::ByteView(certifying_bytes.data(), certifying_bytes.size()));
  for (std::size_t i = 0; i < offenders.size(); ++i) {
    if (i > 0 && !(offenders[i - 1] < offenders[i]))
      throw std::invalid_argument("bft: equivocators must be ascending and unique");
    if (offenders[i] >= current.size()) throw std::invalid_argument("bft: equivocator index out of range");
  }

  const BlockHeader& tip = ledger_->tip();
  if (tip.height == 0) {
    if (cert) throw std::invalid_argument("bft: a genesis tip has no commit certificate");
    if (current.hash() != vset_.hash() || certifying.hash() != vset_.hash())
      throw std::invalid_argument("bft: validator set does not match the genesis set");
  } else {
    if (!cert) throw std::invalid_argument("bft: missing the tip's commit certificate");
    if (cert->height != tip.height || cert->block_id != tip.hash())
      throw std::invalid_argument("bft: commit certificate is not for the restored tip");
    ConsensusData cd;
    try {
      cd = ConsensusData::decode_all(codec::ByteView(tip.consensus_data.data(), tip.consensus_data.size()));
    } catch (const codec::CodecError&) {
      throw std::invalid_argument("bft: the restored tip carries no BFT consensus data");
    }
    if (cd.validator_set_hash != certifying.hash())
      throw std::invalid_argument("bft: certifying set does not match the tip header");
    const std::optional<ValidatorSet> announced = announced_validator_set(tip);
    const Hash expected = announced ? announced->hash() : certifying.hash();
    if (current.hash() != expected)
      throw std::invalid_argument("bft: set in force does not follow from the tip header");
    if (CommitError ce = verify_commit(ledger_->spec().chain_id, certifying, *cert); !ce.ok)
      throw std::invalid_argument("bft: tip certificate rejected: " + ce.error);
  }

  vset_ = std::move(current);
  prev_vset_ = std::move(certifying);
  last_commit_ = std::move(cert);
  pending_ = std::move(pending);
  equivocators_ = std::set<std::uint32_t>(offenders.begin(), offenders.end());
}

Vote BftEngine::precommit(const Block& b, std::uint32_t validator_index,
                          const crypto::KeyPair& bls_key,
                          std::uint32_t round) const {
  // Governance at vote time: never sign for a validator-set change this node
  // did not schedule (or one that differs from what it scheduled).
  if (auto next = announced_validator_set(b.header)) {
    if (!pending_ || pending_->hash() != next->hash())
      throw std::runtime_error("bft: refusing to vote for an unscheduled validator-set change");
  }
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
  if (CommitError ce = validate_consensus_data(ledger_->spec().chain_id, vset_, prev_vset_,
                                               b.header, b.header.parent_hash);
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
    prev_vset_ = vset_;   // this set certified the new tip
    if (auto next = announced_validator_set(b.header)) {  // a finalized rotation takes effect now
      vset_ = std::move(*next);
      pending_.reset();
    }
    if (bus_) {
      bus_->publish(observe::BlockFinalized{b.header.height, block_id(b),
                                            b.header.state_root,
                                            b.transactions.size()});
    }
    if (metrics_) {
      metrics_->gauge("aviary_chain_height", "current finalized height")
          .set(static_cast<std::int64_t>(b.header.height));
      metrics_->counter("aviary_blocks_finalized_total", "blocks finalized").inc();
      metrics_->counter("aviary_transactions_total", "transactions applied")
          .inc(b.transactions.size());
    }
  } else if (bus_) {
    bus_->publish(observe::BlockRejected{b.header.height, block_id(b)});
  }
  return be;
}

}  // namespace aviary::consensus
