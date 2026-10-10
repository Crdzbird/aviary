// Async Tendermint round state machine: an N-validator simulation over an
// in-memory network. Timeouts fire when the network goes quiescent (models
// message delay < timeout). We check agreement (all correct validators decide
// the same value), termination (they DO decide, even with a faulty proposer or
// a silent minority), and safety-relevant behaviour (locking).
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "aviary/round_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;

namespace {
Hash value_for(std::uint32_t proposer) {
  Hash h{};
  h.fill(static_cast<std::uint8_t>(proposer + 1));
  return h;
}

// A deterministic simulated network of round state machines.
struct Sim {
  std::uint32_t n;
  std::unique_ptr<ValidatorSet> vset;
  std::vector<std::unique_ptr<RoundStateMachine>> sm;
  std::set<std::uint32_t> correct;              // validators that follow the protocol
  std::deque<ConsensusMessage> queue;           // in-flight broadcasts (FIFO)
  std::set<std::tuple<std::uint32_t, int, std::uint32_t>> timeouts;  // (round, step, owner)
  std::map<std::uint32_t, Value> decision;

  explicit Sim(std::uint32_t n_) : n(n_) {
    std::vector<Validator> vs;
    for (std::uint32_t i = 0; i < n; ++i) {
      Bytes ikm(32, static_cast<std::uint8_t>(0x40 + i));
      vs.push_back(Validator{crypto::bls_keygen_from_ikm(codec::ByteView(ikm.data(), ikm.size())).public_key, 1});
    }
    vset = std::make_unique<ValidatorSet>(std::move(vs));
    for (std::uint32_t i = 0; i < n; ++i) {
      RoundStateMachine::Config cfg;
      cfg.me = i;
      cfg.validators = vset.get();
      cfg.proposer = [this](std::uint32_t round) { return round % n; };
      cfg.get_value = [i]() { return value_for(i); };
      cfg.valid = [](const Hash&) { return true; };
      sm.push_back(std::make_unique<RoundStateMachine>(std::move(cfg)));
      correct.insert(i);
    }
  }

  void handle(std::uint32_t v, const std::vector<Action>& actions) {
    for (const auto& a : actions) {
      switch (a.kind) {
        case Action::Kind::kBroadcastProposal:
          queue.push_back({ConsensusMessage::Kind::kProposal, a.round, v, a.value, a.valid_round});
          break;
        case Action::Kind::kBroadcastPrevote:
          queue.push_back({ConsensusMessage::Kind::kPrevote, a.round, v, a.value, -1});
          break;
        case Action::Kind::kBroadcastPrecommit:
          queue.push_back({ConsensusMessage::Kind::kPrecommit, a.round, v, a.value, -1});
          break;
        case Action::Kind::kScheduleTimeout:
          timeouts.insert({a.round, static_cast<int>(a.timeout_step), v});
          break;
        case Action::Kind::kDecide:
          decision[v] = a.value;
          break;
      }
    }
  }

  void deliver(const ConsensusMessage& m) {
    for (std::uint32_t v : correct) handle(v, sm[v]->receive(m));
  }

  // Run until every correct validator has decided, or a step budget is hit.
  void run(int budget = 100000) {
    for (std::uint32_t v : correct) handle(v, sm[v]->start());
    int steps = 0;
    while (steps++ < budget) {
      bool all = true;
      for (std::uint32_t v : correct) all &= decision.count(v) != 0;
      if (all) return;
      if (!queue.empty()) {
        ConsensusMessage m = queue.front();
        queue.pop_front();
        deliver(m);
      } else if (!timeouts.empty()) {
        auto [round, step, owner] = *timeouts.begin();  // fire the earliest
        timeouts.erase(timeouts.begin());
        if (correct.count(owner)) handle(owner, sm[owner]->on_timeout(static_cast<Step>(step), round));
      } else {
        return;  // stuck (no messages, no timeouts) — will show as an undecided validator
      }
    }
  }

  bool all_decided() const {
    for (std::uint32_t v : correct)
      if (decision.count(v) == 0) return false;
    return true;
  }

  bool agreement() const {  // all correct validators decided the same value
    Value first;
    bool have = false;
    for (std::uint32_t v : correct) {
      auto it = decision.find(v);
      if (it == decision.end()) return false;
      if (!have) { first = it->second; have = true; }
      else if (it->second != first) return false;
    }
    return true;
  }
};
}  // namespace

int main() {
  init();

  std::printf("== happy path: 4 correct validators decide the proposer's value ==\n");
  {
    Sim s(4);
    s.run();
    CHECK(s.all_decided());
    CHECK(s.agreement());
    // Round-0 proposer is validator 0 → everyone decides value_for(0).
    CHECK(s.decision[0].has_value() && *s.decision[0] == value_for(0));
  }

  std::printf("== faulty round-0 proposer: validators round-change and still decide ==\n");
  {
    Sim s(4);
    s.correct.erase(0);         // validator 0 is silent (never proposes/votes)
    s.decision.erase(0);
    s.run();
    CHECK(s.all_decided());     // the other three still terminate
    CHECK(s.agreement());
    // Round 0 stalls (no proposal); round-1 proposer is validator 1.
    CHECK(s.decision[1].has_value() && *s.decision[1] == value_for(1));
    CHECK(s.sm[1]->round() >= 1);  // advanced past the failed round
  }

  std::printf("== silent minority (f=2 of 7): the 5 honest still reach consensus ==\n");
  {
    Sim s(7);                   // quorum = 5
    s.correct.erase(5);
    s.correct.erase(6);
    s.decision.erase(5);
    s.decision.erase(6);
    s.run();
    CHECK(s.all_decided());
    CHECK(s.agreement());
  }

  std::printf("== larger set converges (10 correct validators) ==\n");
  {
    Sim s(10);
    s.run();
    CHECK(s.all_decided());
    CHECK(s.agreement());
    CHECK(s.decision[0].has_value() && *s.decision[0] == value_for(0));
  }

  std::printf("== locking: a validator locks the value it precommits ==\n");
  {
    Sim s(4);
    s.run();
    // After deciding value_for(0), every correct validator locked that value in
    // the round it precommitted.
    for (std::uint32_t v = 0; v < 4; ++v) {
      CHECK(s.sm[v]->locked_value().has_value());
      CHECK(*s.sm[v]->locked_value() == value_for(0));
      CHECK(s.sm[v]->locked_round() >= 0);
    }
  }

  return bctest::summary();
}
