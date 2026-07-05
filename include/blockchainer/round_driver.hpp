#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/round_state.hpp"
#include "blockchainer/wire.hpp"

/// Networked driver for the async Tendermint round state machine (ARCHITECTURE
/// §7). It binds the pure [RoundStateMachine] to the real world: block
/// production/validation/commit through the [Ledger], BLS vote signing and
/// verification against the validator set, [CommitCertificate] aggregation on
/// decide, and gossip via [wire::ConsensusMsg]. The state machine decides *what*
/// to do; the driver does it.
///
/// Scope: a single height (the one extending the ledger tip). It translates each
/// SM [Action] into signed wire messages + timeout requests, and each inbound
/// wire message (after signature verification) into an SM input. Timers and the
/// network are the caller's: the driver returns the messages to gossip and the
/// timeouts to arm, and the caller feeds back deliveries and fired timeouts —
/// keeping the whole thing deterministic and testable, exactly like the SM.
namespace blockchainer::consensus {

class RoundDriver {
 public:
  /// A timeout the caller should arm; when it fires, call on_timeout(step, round).
  struct TimeoutRequest {
    Step step;
    std::uint32_t round;
  };

  /// What the driver wants done after processing an input.
  struct Output {
    std::vector<wire::Message> broadcasts;   ///< consensus messages to gossip to all peers
    std::vector<TimeoutRequest> timeouts;    ///< timers to arm
  };

  RoundDriver(std::uint64_t chain_id, ValidatorSet vset, std::uint32_t me,
              crypto::KeyPair bls_key, Ledger* ledger, std::uint64_t timestamp);

  /// Begin consensus for this height (StartRound(0)).
  Output start();
  /// Ingest a peer's consensus message (verifies signatures; drops bad ones).
  Output deliver(const wire::ConsensusMsg& msg);
  /// Fire a previously-armed timeout.
  Output on_timeout(Step step, std::uint32_t round);

  /// After committing, roll into the next height: carry this height's commit
  /// certificate as the next block's LastCommit, reset the round state machine,
  /// and StartRound(0). Precondition: committed(). Returns the new height's
  /// start actions.
  Output advance();

  bool committed() const { return committed_; }
  std::uint32_t round() const { return sm_.round(); }
  std::uint64_t height() const { return height_; }
  const std::optional<CommitCertificate>& certificate() const { return cert_; }

 private:
  RoundStateMachine::Config make_config();
  Output run(const std::vector<Action>& actions);
  Hash get_value();                 // build a block extending the tip; store it
  bool valid(const Hash& block_id); // do we hold a block that legally extends the tip
  void commit(std::uint32_t round, const Value& value);

  std::uint64_t chain_id_;
  ValidatorSet vset_;
  std::uint32_t me_;
  crypto::KeyPair bls_;
  Ledger* ledger_;
  std::uint64_t timestamp_;
  std::uint64_t height_;  // the height being decided (tip + 1)

  RoundStateMachine sm_;
  // The certificate that finalized the parent — embedded as this height's block's
  // LastCommit. Absent for the first block after genesis.
  std::optional<CommitCertificate> last_commit_;
  std::map<Hash, Block> blocks_;  // block id → block (self-produced or from a proposal)
  // Precommit signatures collected for cert building: round → value → validator → sig.
  // A nil precommit uses the zero-hash sentinel value.
  std::map<std::uint32_t, std::map<Hash, std::map<std::uint32_t, Bytes>>> precommit_sigs_;
  bool committed_ = false;
  std::optional<CommitCertificate> cert_;
};

}  // namespace blockchainer::consensus
