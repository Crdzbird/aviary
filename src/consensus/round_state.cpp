#include "aviary/round_state.hpp"

#include <utility>

namespace aviary::consensus {

namespace {
Action broadcast(ConsensusMessage::Kind kind, std::uint32_t round, Value value,
                 std::int64_t valid_round = -1) {
  Action a;
  a.round = round;
  a.value = std::move(value);
  a.valid_round = valid_round;
  switch (kind) {
    case ConsensusMessage::Kind::kProposal: a.kind = Action::Kind::kBroadcastProposal; break;
    case ConsensusMessage::Kind::kPrevote: a.kind = Action::Kind::kBroadcastPrevote; break;
    case ConsensusMessage::Kind::kPrecommit: a.kind = Action::Kind::kBroadcastPrecommit; break;
  }
  return a;
}

Action schedule(Step step, std::uint32_t round) {
  Action a;
  a.kind = Action::Kind::kScheduleTimeout;
  a.timeout_step = step;
  a.round = round;
  return a;
}
}  // namespace

std::uint64_t RoundStateMachine::power_of(const std::set<std::uint32_t>& s) const {
  std::uint64_t p = 0;
  for (std::uint32_t i : s) {
    if (i < cfg_.validators->size()) p += cfg_.validators->at(i).power;
  }
  return p;
}

std::uint64_t RoundStateMachine::quorum() const { return cfg_.validators->quorum(); }

std::uint64_t RoundStateMachine::f_plus_1() const {
  return cfg_.validators->total_power() / 3 + 1;
}

void RoundStateMachine::start_round(std::uint32_t round, std::vector<Action>& out) {
  round_ = round;
  step_ = Step::kPropose;
  if (is_proposer(round_)) {
    Hash proposal = valid_value_.has_value() ? *valid_value_ : cfg_.get_value();
    out.push_back(broadcast(ConsensusMessage::Kind::kProposal, round_, Value{proposal},
                            valid_round_));
  } else {
    out.push_back(schedule(Step::kPropose, round_));
  }
}

std::vector<Action> RoundStateMachine::start() {
  std::vector<Action> out;
  start_round(0, out);
  try_transitions(out);
  return out;
}

std::vector<Action> RoundStateMachine::receive(const ConsensusMessage& msg) {
  std::vector<Action> out;
  any_msg_[msg.round].insert(msg.sender);
  switch (msg.kind) {
    case ConsensusMessage::Kind::kProposal:
      // Only the round's proposer can propose.
      if (msg.sender == cfg_.proposer(msg.round)) {
        proposals_[msg.round] = Proposal{msg.value, msg.valid_round, true};
      }
      break;
    case ConsensusMessage::Kind::kPrevote:
      prevotes_[msg.round][msg.value].insert(msg.sender);
      break;
    case ConsensusMessage::Kind::kPrecommit:
      precommits_[msg.round][msg.value].insert(msg.sender);
      break;
  }
  try_transitions(out);
  return out;
}

std::vector<Action> RoundStateMachine::on_timeout(Step step, std::uint32_t round) {
  std::vector<Action> out;
  if (round == round_) {
    if (step == Step::kPropose && step_ == Step::kPropose) {
      out.push_back(broadcast(ConsensusMessage::Kind::kPrevote, round_, Value{}));  // prevote nil
      step_ = Step::kPrevote;
    } else if (step == Step::kPrevote && step_ == Step::kPrevote) {
      out.push_back(broadcast(ConsensusMessage::Kind::kPrecommit, round_, Value{}));  // precommit nil
      step_ = Step::kPrecommit;
    } else if (step == Step::kPrecommit) {
      start_round(round_ + 1, out);
    }
  }
  try_transitions(out);
  return out;
}

void RoundStateMachine::try_transitions(std::vector<Action>& out) {
  // Distinct-sender power across every value bucket for a round (a validator's
  // power counts once even if it voted for multiple values).
  auto any_power = [this](const std::map<Value, std::set<std::uint32_t>>& buckets) {
    std::set<std::uint32_t> all;
    for (const auto& [v, senders] : buckets) all.insert(senders.begin(), senders.end());
    return power_of(all);
  };

  bool progress = true;
  while (progress) {
    progress = false;

    // Rule (line 22): PROPOSAL in current round with vr = -1, while step = propose.
    if (step_ == Step::kPropose) {
      auto it = proposals_.find(round_);
      if (it != proposals_.end() && it->second.present && it->second.valid_round == -1) {
        const Value& v = it->second.value;
        const bool ok = v.has_value() && cfg_.valid(*v) &&
                        (locked_round_ == -1 || locked_value_ == v);
        out.push_back(broadcast(ConsensusMessage::Kind::kPrevote, round_, ok ? v : Value{}));
        step_ = Step::kPrevote;
        progress = true;
        continue;
      }
    }

    // Rule (line 28): PROPOSAL with valid vr AND 2f+1 prevotes for id(v) in vr,
    // while step = propose and 0 <= vr < round.
    if (step_ == Step::kPropose) {
      auto it = proposals_.find(round_);
      if (it != proposals_.end() && it->second.present) {
        const std::int64_t vr = it->second.valid_round;
        const Value& v = it->second.value;
        if (vr >= 0 && vr < static_cast<std::int64_t>(round_) && v.has_value()) {
          auto pit = prevotes_.find(static_cast<std::uint32_t>(vr));
          std::uint64_t p = 0;
          if (pit != prevotes_.end()) {
            auto vit = pit->second.find(v);
            if (vit != pit->second.end()) p = power_of(vit->second);
          }
          if (p >= quorum()) {
            const bool ok = cfg_.valid(*v) && (locked_round_ <= vr || locked_value_ == v);
            out.push_back(broadcast(ConsensusMessage::Kind::kPrevote, round_, ok ? v : Value{}));
            step_ = Step::kPrevote;
            progress = true;
            continue;
          }
        }
      }
    }

    // Rule (line 34): 2f+1 prevotes (any value) in current round, while
    // step = prevote, the first time → schedule the prevote timeout.
    if (step_ == Step::kPrevote && prevote_timeout_armed_.count(round_) == 0) {
      auto pit = prevotes_.find(round_);
      if (pit != prevotes_.end() && any_power(pit->second) >= quorum()) {
        out.push_back(schedule(Step::kPrevote, round_));
        prevote_timeout_armed_.insert(round_);
        progress = true;
        continue;
      }
    }

    // Rule (line 36): PROPOSAL in current round AND 2f+1 prevotes for id(v),
    // valid(v), while step >= prevote, the first time this round.
    if (step_ >= Step::kPrevote && upon_prevote_quorum_done_.count(round_) == 0) {
      auto it = proposals_.find(round_);
      if (it != proposals_.end() && it->second.present && it->second.value.has_value()) {
        const Value& v = it->second.value;
        auto pit = prevotes_.find(round_);
        std::uint64_t p = 0;
        if (pit != prevotes_.end()) {
          auto vit = pit->second.find(v);
          if (vit != pit->second.end()) p = power_of(vit->second);
        }
        if (cfg_.valid(*v) && p >= quorum()) {
          if (step_ == Step::kPrevote) {
            locked_value_ = v;
            locked_round_ = static_cast<std::int64_t>(round_);
            out.push_back(broadcast(ConsensusMessage::Kind::kPrecommit, round_, v));
            step_ = Step::kPrecommit;
          }
          valid_value_ = v;
          valid_round_ = static_cast<std::int64_t>(round_);
          upon_prevote_quorum_done_.insert(round_);
          progress = true;
          continue;
        }
      }
    }

    // Rule (line 44): 2f+1 prevotes for nil in current round, while step = prevote.
    if (step_ == Step::kPrevote) {
      auto pit = prevotes_.find(round_);
      if (pit != prevotes_.end()) {
        auto nit = pit->second.find(Value{});
        if (nit != pit->second.end() && power_of(nit->second) >= quorum()) {
          out.push_back(broadcast(ConsensusMessage::Kind::kPrecommit, round_, Value{}));
          step_ = Step::kPrecommit;
          progress = true;
          continue;
        }
      }
    }

    // Rule (line 47): 2f+1 precommits (any value) in current round, the first
    // time → schedule the precommit timeout.
    if (precommit_timeout_armed_.count(round_) == 0) {
      auto pit = precommits_.find(round_);
      if (pit != precommits_.end() && any_power(pit->second) >= quorum()) {
        out.push_back(schedule(Step::kPrecommit, round_));
        precommit_timeout_armed_.insert(round_);
        progress = true;
        continue;
      }
    }

    // Rule (line 49): PROPOSAL at any round r AND 2f+1 precommits for id(v) at r,
    // while not yet decided → decide v.
    if (!decision_.has_value()) {
      bool decided_now = false;
      for (const auto& [r, prop] : proposals_) {
        if (!prop.present || !prop.value.has_value()) continue;
        auto pit = precommits_.find(r);
        if (pit == precommits_.end()) continue;
        auto vit = pit->second.find(prop.value);
        if (vit == pit->second.end()) continue;
        if (power_of(vit->second) >= quorum() &&
            (cfg_.finalizable ? cfg_.finalizable(*prop.value) : cfg_.valid(*prop.value))) {
          decision_ = prop.value;
          Action d;
          d.kind = Action::Kind::kDecide;
          d.round = r;
          d.value = prop.value;
          out.push_back(d);
          decided_now = true;
          break;
        }
      }
      if (decided_now) {
        progress = true;
        continue;
      }
    }

    // Rule (line 55): f+1 messages from a round > current → skip to it.
    if (!decision_.has_value()) {
      for (const auto& [r, senders] : any_msg_) {
        if (r > round_ && power_of(senders) >= f_plus_1()) {
          start_round(r, out);
          progress = true;
          break;
        }
      }
      if (progress) continue;
    }
  }
}

}  // namespace aviary::consensus
