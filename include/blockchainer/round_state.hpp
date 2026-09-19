#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/crypto.hpp"

/// Asynchronous Tendermint round state machine (ARCHITECTURE §7; the "latest
/// gossip on BFT consensus" Algorithm 1). This is the *liveness* machinery that
/// sits on top of the safety-critical certificate core in `consensus.hpp`:
/// propose / prevote / precommit steps, deterministic proposer rotation,
/// round-change on timeouts, and the locking (proof-of-lock-change) rules that
/// keep safety across rounds.
///
/// Deliberately a **pure** state machine for one validator at one height: no
/// clock, no randomness, no I/O, no crypto. It consumes already-authenticated
/// messages and timeout events and returns the [Action]s the driver must carry
/// out (broadcast a message, schedule a timeout, or decide). That keeps it
/// deterministic and exhaustively testable — the same property the rest of the
/// consensus layer relies on. Signing, verification, and networking live in the
/// driver above it.
namespace blockchainer::consensus {

using crypto::Hash;

enum class Step : std::uint8_t { kPropose = 0, kPrevote = 1, kPrecommit = 2 };

/// A consensus value is a block id, or `nil` (nullopt).
using Value = std::optional<Hash>;

/// An authenticated consensus message handed to the state machine.
struct ConsensusMessage {
  enum class Kind : std::uint8_t { kProposal, kPrevote, kPrecommit };
  Kind kind = Kind::kPrevote;
  std::uint32_t round = 0;
  std::uint32_t sender = 0;   ///< validator index
  Value value{};              ///< proposal: the value; vote: id(v) or nil
  std::int64_t valid_round = -1;  ///< proposal only (the vr field); else -1
};

/// An effect the state machine asks the driver to perform.
struct Action {
  enum class Kind : std::uint8_t {
    kBroadcastProposal,
    kBroadcastPrevote,
    kBroadcastPrecommit,
    kScheduleTimeout,
    kDecide,
  };
  Kind kind = Kind::kScheduleTimeout;
  std::uint32_t round = 0;
  Value value{};                        ///< proposal / vote / decided value
  std::int64_t valid_round = -1;        ///< proposal broadcast: our validRound
  Step timeout_step = Step::kPropose;   ///< schedule-timeout: which step
};

/// Tendermint round state machine for a single height.
class RoundStateMachine {
 public:
  struct Config {
    std::uint32_t me = 0;                         ///< this validator's index
    const ValidatorSet* validators = nullptr;     ///< power + quorum source
    std::function<std::uint32_t(std::uint32_t round)> proposer;  ///< proposer(round)
    std::function<Hash()> get_value;              ///< a fresh value to propose
    std::function<bool(const Hash&)> valid;       ///< is a proposed value acceptable (to VOTE for)
    /// May this value be DECIDED once 2f+1 precommit it? Defaults to `valid`.
    /// Split so a node can refuse to vote for something (governance, e.g. an
    /// unscheduled validator-set change) yet still follow the network's
    /// decision when a quorum certifies it — otherwise it would strand itself.
    std::function<bool(const Hash&)> finalizable;
  };

  explicit RoundStateMachine(Config cfg) : cfg_(std::move(cfg)) {}

  std::uint32_t round() const { return round_; }
  Step step() const { return step_; }
  bool decided() const { return decision_.has_value(); }
  Value decision() const { return decision_; }
  Value locked_value() const { return locked_value_; }
  std::int64_t locked_round() const { return locked_round_; }

  /// Begin consensus (StartRound(0)).
  std::vector<Action> start();
  /// Feed an authenticated message; returns resulting actions.
  std::vector<Action> receive(const ConsensusMessage& msg);
  /// Fire a scheduled timeout for (step, round).
  std::vector<Action> on_timeout(Step step, std::uint32_t round);

 private:
  Config cfg_;

  std::uint32_t round_ = 0;
  Step step_ = Step::kPropose;
  Value decision_{};

  Value locked_value_{};
  std::int64_t locked_round_ = -1;
  Value valid_value_{};
  std::int64_t valid_round_ = -1;

  // Received messages, indexed for the "upon 2f+1 …" queries.
  struct Proposal {
    Value value{};
    std::int64_t valid_round = -1;
    bool present = false;
  };
  std::map<std::uint32_t, Proposal> proposals_;                        // round → proposer's proposal
  std::map<std::uint32_t, std::map<Value, std::set<std::uint32_t>>> prevotes_;   // round → value → senders
  std::map<std::uint32_t, std::map<Value, std::set<std::uint32_t>>> precommits_; // round → value → senders
  std::map<std::uint32_t, std::set<std::uint32_t>> any_msg_;           // round → senders (round-skip)

  // "for the first time" guards.
  std::set<std::uint32_t> prevote_timeout_armed_;
  std::set<std::uint32_t> precommit_timeout_armed_;
  std::set<std::uint32_t> upon_prevote_quorum_done_;  // line 36 rule, once per round

  // Helpers.
  std::uint64_t power_of(const std::set<std::uint32_t>& s) const;
  std::uint64_t quorum() const;   // 2f+1 (by power)
  std::uint64_t f_plus_1() const; // f+1  (by power)
  bool is_proposer(std::uint32_t round) const { return cfg_.proposer(round) == cfg_.me; }

  void start_round(std::uint32_t round, std::vector<Action>& out);
  void try_transitions(std::vector<Action>& out);  // re-evaluate all "upon" rules to fixpoint
};

}  // namespace blockchainer::consensus
