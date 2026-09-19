#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/gossip.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/round_driver.hpp"

/// A runnable validator node (ARCHITECTURE §7/§9): the assembly that ties the
/// consensus driver to the gossip overlay over its own ledger. It is the first
/// object that behaves like a whole validator — everything below (round state
/// machine, BLS votes, commit certificates, mesh flooding, dedup) is a component
/// it wires together.
///
/// It bridges two seams: a consensus [RoundDriver] that emits/consumes
/// `wire::ConsensusMsg`, and a [GossipOverlay] that floods opaque bytes across a
/// partial mesh. A consensus broadcast becomes a gossip origination; a gossip
/// delivery (deduped by content hash) feeds the driver AND relays onward. So
/// consensus runs over a realistic relayed network instead of magic broadcast —
/// which is what lets it show relay tolerance (a validator reaches quorum via
/// hops) and partition tolerance (a connected ≥quorum majority still finalizes).
///
/// Still a pure input/output object (no clock/socket): it returns the gossip
/// relays to send and the timeouts to arm; the caller wires them to a real
/// socket + timer or an in-memory harness. Swapping that transport for real TCP
/// + the mutually-authenticated secure session is then a mechanical change.
namespace blockchainer::p2p {

class NetworkedNode {
 public:
  /// What the node wants done after processing an input.
  struct Outbound {
    std::vector<GossipOverlay::Relay> relays;                     ///< gossip: (peer, bytes)
    std::vector<consensus::RoundDriver::TimeoutRequest> timeouts;  ///< timers to arm
  };

  NetworkedNode(std::uint64_t chain_id, consensus::ValidatorSet vset, std::uint32_t me,
                crypto::KeyPair bls_key, std::unique_ptr<Ledger> ledger,
                std::uint64_t timestamp)
      : ledger_(std::move(ledger)),
        driver_(chain_id, std::move(vset), me, std::move(bls_key), ledger_.get(), timestamp),
        gossip_(me) {}

  std::uint32_t id() const { return gossip_.self(); }
  /// Parent chains: anchors to include when this node proposes (see RoundDriver).
  void set_anchors(std::vector<Anchor> anchors) { driver_.set_anchors(std::move(anchors)); }
  /// Coordinated validator-set change (see RoundDriver::schedule_validator_set).
  void schedule_validator_set(consensus::ValidatorSet next) { driver_.schedule_validator_set(std::move(next)); }
  /// The validator set in force for the next height.
  const consensus::ValidatorSet& validators() const { return driver_.validators(); }
  void add_peer(std::uint32_t peer) { gossip_.add_peer(peer); }

  /// Begin consensus (StartRound(0)); consensus broadcasts are gossiped.
  Outbound start();

  /// Handle a gossip payload received from peer [from]. If new, feed the driver,
  /// relay it onward, and gossip any resulting consensus messages; if already
  /// seen, drop.
  Outbound on_gossip(std::uint32_t from, const Bytes& payload);

  /// Fire a previously-armed consensus timeout.
  Outbound on_timeout(consensus::Step step, std::uint32_t round);

  bool committed() const { return driver_.committed(); }
  std::uint64_t height() const { return driver_.height(); }
  const Ledger& ledger() const { return *ledger_; }

  /// Whether a peer has been banned for misbehavior (e.g. repeated garbage).
  bool peer_banned(std::uint32_t peer) const { return gossip_.banned(peer); }

  /// Every unique consensus payload this node has originated or accepted for the
  /// current height, in arrival order. A transport replays this to a peer whose
  /// link (re)establishes after consensus started — reconnect catch-up. Votes
  /// lost on a dead socket are never re-flooded by gossip (dedup), so without
  /// this a healed partition stays stalled: neither side can reach quorum, and
  /// Tendermint can't even change rounds without 2f+1 crossing the cut. The
  /// receiver's dedup makes replay idempotent. Bounded: one height's messages.
  const std::vector<Bytes>& replay_log() const { return replay_; }

 private:
  Outbound emit(const consensus::RoundDriver::Output& out);  // driver output → gossip + timeouts

  std::unique_ptr<Ledger> ledger_;
  consensus::RoundDriver driver_;
  GossipOverlay gossip_;
  std::vector<Bytes> replay_;  // unique consensus payloads this height (catch-up)
};

}  // namespace blockchainer::p2p
