#include "blockchainer/round_driver.hpp"

#include <utility>

namespace blockchainer::consensus {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

RoundStateMachine::Config RoundDriver::make_config() {
  RoundStateMachine::Config cfg;
  cfg.me = me_;
  cfg.validators = &vset_;
  const std::uint32_t n = static_cast<std::uint32_t>(vset_.size());
  cfg.proposer = [n](std::uint32_t round) { return n == 0 ? 0 : round % n; };
  cfg.get_value = [this]() { return get_value(); };
  cfg.valid = [this](const Hash& id) { return valid(id); };
  cfg.finalizable = [this](const Hash& id) { return applicable(id); };
  return cfg;
}

RoundDriver::RoundDriver(std::uint64_t chain_id, ValidatorSet vset, std::uint32_t me,
                         crypto::KeyPair bls_key, Ledger* ledger, std::uint64_t timestamp)
    : chain_id_(chain_id),
      vset_(std::move(vset)),
      me_(me),
      bls_(std::move(bls_key)),
      ledger_(ledger),
      timestamp_(timestamp),
      height_(ledger->tip().height + 1),
      sm_(make_config()),
      prev_vset_(vset_) {}

Hash RoundDriver::get_value() {
  // Build an empty block extending the tip; stamp consensus_data with the
  // validator-set hash and the LastCommit (absent for the first block).
  Block b = ledger_->build_block({}, timestamp_, anchors_);
  ConsensusData cd{vset_.hash(), last_commit_};
  if (pending_) cd.next_validators = pending_->validators();
  b.header.consensus_data = cd.encode();
  const Hash id = b.header.hash();
  blocks_[id] = std::move(b);
  return id;
}

bool RoundDriver::applicable(const Hash& block_id) {
  auto it = blocks_.find(block_id);
  if (it == blocks_.end()) return false;  // we don't hold the block
  const BlockHeader& h = it->second.header;
  if (h.height != height_) return false;
  if (h.parent_hash != ledger_->tip().hash()) return false;
  if (!validate_consensus_data(chain_id_, vset_, prev_vset_, h, ledger_->tip().hash()).ok) return false;
  // Never accept a block the ledger would refuse at commit (txs, roots,
  // subchain anchors): a certified-but-unapplicable block would strand the
  // height. Full dry run, no state change.
  return ledger_->validate_block(it->second).ok;
}

bool RoundDriver::valid(const Hash& block_id) {
  if (!applicable(block_id)) return false;
  // Governance at vote time: only a validator-set change this node scheduled
  // gets our vote. (A certified one is still FOLLOWED — see Config::finalizable.)
  auto it = blocks_.find(block_id);
  if (auto next = announced_validator_set(it->second.header)) {
    if (!pending_ || pending_->hash() != next->hash()) return false;
  }
  return true;
}

RoundDriver::Output RoundDriver::run(const std::vector<Action>& actions) {
  Output out;
  for (const auto& a : actions) {
    // A node rotated out of the set keeps following (delivering, committing)
    // but must not propose or vote: it has no index to sign under.
    if (!is_validator() && (a.kind == Action::Kind::kBroadcastProposal ||
                            a.kind == Action::Kind::kBroadcastPrevote ||
                            a.kind == Action::Kind::kBroadcastPrecommit))
      continue;
    switch (a.kind) {
      case Action::Kind::kBroadcastProposal: {
        wire::ConsensusMsg c;
        c.kind = 0;
        c.height = height_;
        c.round = a.round;
        c.sender = me_;
        c.has_value = a.value.has_value();
        c.valid_round = a.valid_round;
        if (a.value) {
          c.block_id = *a.value;
          if (auto it = blocks_.find(*a.value); it != blocks_.end()) c.block = it->second.encode();
        }
        out.broadcasts.push_back(wire::make(c));
        break;
      }
      case Action::Kind::kBroadcastPrevote:
      case Action::Kind::kBroadcastPrecommit: {
        const bool precommit = a.kind == Action::Kind::kBroadcastPrecommit;
        const Hash bid = a.value ? *a.value : crypto::zero_hash();  // zero = nil
        wire::ConsensusMsg c;
        c.kind = precommit ? 2 : 1;
        c.height = height_;
        c.round = a.round;
        c.sender = me_;
        c.has_value = a.value.has_value();
        c.block_id = bid;
        // Sign the vote with our BLS key over the canonical vote bytes (index-bound).
        const Bytes signing = vote_signing_bytes(chain_id_, height_, a.round, bid, me_);
        c.signature = crypto::bls_sign(view(signing), view(bls_.secret_key));
        out.broadcasts.push_back(wire::make(c));
        break;
      }
      case Action::Kind::kScheduleTimeout:
        out.timeouts.push_back({a.timeout_step, a.round});
        break;
      case Action::Kind::kDecide:
        commit(a.round, a.value);
        break;
    }
  }
  return out;
}

RoundDriver::Output RoundDriver::start() { return run(sm_.start()); }

RoundDriver::Output RoundDriver::deliver(const wire::ConsensusMsg& msg) {
  if (msg.height != height_) return {};

  ConsensusMessage cm;
  cm.round = msg.round;
  cm.sender = msg.sender;
  cm.valid_round = msg.valid_round;

  if (msg.kind == 0) {  // proposal
    if (!msg.block.empty()) {
      try {
        Block b = Block::decode_all(view(msg.block));
        if (b.header.hash() == msg.block_id) blocks_[msg.block_id] = std::move(b);
      } catch (...) {
        return {};  // malformed block → ignore the proposal
      }
    }
    cm.kind = ConsensusMessage::Kind::kProposal;
    cm.value = msg.has_value ? Value{msg.block_id} : Value{};
  } else {  // prevote / precommit
    if (msg.sender >= vset_.size()) return {};
    const bool precommit = msg.kind == 2;
    const Hash bid = msg.has_value ? msg.block_id : crypto::zero_hash();
    const Bytes signing = vote_signing_bytes(chain_id_, height_, msg.round, bid, msg.sender);
    if (!crypto::bls_verify(view(signing), view(msg.signature),
                            view(vset_.at(msg.sender).bls_pubkey))) {
      return {};  // bad signature → drop
    }
    if (precommit) precommit_sigs_[msg.round][bid][msg.sender] = msg.signature;
    cm.kind = precommit ? ConsensusMessage::Kind::kPrecommit : ConsensusMessage::Kind::kPrevote;
    cm.value = msg.has_value ? Value{msg.block_id} : Value{};
  }
  return run(sm_.receive(cm));
}

RoundDriver::Output RoundDriver::on_timeout(Step step, std::uint32_t round) {
  return run(sm_.on_timeout(step, round));
}

RoundDriver::Output RoundDriver::advance() {
  if (!committed_) return {};  // nothing finalized yet
  last_commit_ = cert_;        // certifies the block we just committed
  height_ = ledger_->tip().height + 1;
  sm_ = RoundStateMachine(make_config());
  blocks_.clear();
  precommit_sigs_.clear();
  committed_ = false;
  cert_.reset();
  return run(sm_.start());
}

void RoundDriver::commit(std::uint32_t round, const Value& value) {
  if (committed_ || !value.has_value()) return;
  const Hash id = *value;
  auto bit = blocks_.find(id);
  if (bit == blocks_.end()) return;  // we never learned the block

  // Aggregate the precommit signatures we collected for (round, id) into a
  // >2/3 commit certificate, then apply the block atomically.
  std::vector<Vote> votes;
  if (auto rit = precommit_sigs_.find(round); rit != precommit_sigs_.end()) {
    if (auto vit = rit->second.find(id); vit != rit->second.end()) {
      for (const auto& [idx, sig] : vit->second) votes.push_back(Vote{idx, sig});
    }
  }
  std::optional<CommitCertificate> cert =
      aggregate_commit(chain_id_, vset_, height_, round, id, votes);
  if (!cert) return;  // not enough valid precommits (should not happen at decide)

  if (ledger_->process_block(bit->second).ok) {
    cert_ = std::move(cert);
    committed_ = true;
    // A finalized rotation takes effect NOW for the next height: validators()
    // answers "in force next" immediately, and any late message for this
    // (decided) height simply no longer verifies — harmless.
    prev_vset_ = vset_;  // this set certified the new tip
    if (auto next = announced_validator_set(bit->second.header)) {
      vset_ = std::move(*next);
      pending_.reset();
      auto idx = vset_.index_of(bls_.public_key);
      me_ = idx ? *idx : kNotValidator;  // our index may have moved — or we may be out
    }
  }
}

}  // namespace blockchainer::consensus
