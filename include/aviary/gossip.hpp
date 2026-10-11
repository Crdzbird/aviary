#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

namespace aviary::consensus {
class CredentialGate;
}  // namespace aviary::consensus

/// Gossip overlay (ARCHITECTURE section 9). The wire protocol and secure sessions carry
/// messages between two peers; this is the mesh on top: each node dedups
/// messages by content hash and floods a new one to its peers (except whoever
/// sent it), so a message reaches every connected node exactly once, without
/// loops or floods, over an arbitrary (partial) topology.
///
/// Hardening (so an adversary can't exhaust memory or dominate the mesh):
///  - the seen-set is **bounded** - a capacity-limited FIFO window, so it never
///     grows without limit. Eviction is by insertion order (deterministic and
///     clock-free - the core forbids wall-clock); the window must exceed the
///     in-flight message count so a message fully propagates before it ages out.
///  - peers are **scored**: the caller reports misbehavior (e.g. an undecodable
///     or invalidly-signed payload) via `penalize`, and once a peer crosses the
///     ban threshold it is dropped - no longer relayed to, and its messages are
///     ignored.
///
/// Transport-agnostic and deterministic: the overlay returns the relays it wants
/// sent; the caller wires them to a real socket or the in-memory transport.
namespace aviary::p2p {

class GossipOverlay {
 public:
  /// Overlay limits. Defaults are generous enough for a small validator mesh.
  struct Config {
    std::size_t seen_capacity = 8192;  ///< max remembered message ids (FIFO window)
    int ban_threshold = 50;            ///< cumulative misbehavior weight before a peer is banned
  };

  explicit GossipOverlay(std::uint32_t self) : GossipOverlay(self, Config{}) {}
  GossipOverlay(std::uint32_t self, Config cfg) : self_(self), cfg_(cfg) {}

  /// Add a peer to the mesh. This is the TRUSTED path - not membership-gated even
  /// when a gate is attached (use it for genesis-known / already-authenticated
  /// peers).
  void add_peer(std::uint32_t peer) {
    if (peer != self_ && !banned_.count(peer)) peers_.insert(peer);
  }

  /// Attach an anonymous-membership admission gate (nullable). When set,
  /// [admit_peer] requires a valid "gossip"-scope presentation before a peer
  /// joins the mesh (docs/design/CREDENTIAL_GATE.md, Option A). The gate is only
  /// read (admit() is const); ownership stays with the caller.
  void set_credential_gate(const consensus::CredentialGate* gate) { gate_ = gate; }

  /// Admit a peer behind a membership proof. When a gate is attached, the
  /// [presentation] must admit under scope "gossip" bound to ([epoch],
  /// [challenge], [now]) or the peer is NOT added and this returns false. With no
  /// gate attached it is exactly [add_peer] and returns true (the presentation is
  /// ignored), so an ungated (permissionless) mesh is unaffected. The gate is a
  /// *local* admission policy - [now] is caller-supplied, keeping this clock-free.
  bool admit_peer(std::uint32_t peer, std::uint32_t epoch, crypto::ByteView challenge,
                  crypto::ByteView presentation, std::uint64_t now);

  std::uint32_t self() const { return self_; }
  const std::set<std::uint32_t>& peers() const { return peers_; }

  /// A message to relay to a specific peer.
  struct Relay {
    std::uint32_t to;
    Bytes message;
  };

  /// Originate [message] from this node -> relay it to every peer.
  std::vector<Relay> originate(const Bytes& message);

  /// Handle [message] received from peer [from]. If [from] is banned, drop it.
  /// If it is new, mark it seen and relay it to every peer except [from]
  /// (returned); if already seen, return nothing (dedup - stops loops/floods).
  std::vector<Relay> on_receive(std::uint32_t from, const Bytes& message);

  /// Whether this node has already processed [message] (within the window).
  bool seen(const Bytes& message) const;
  std::size_t seen_count() const { return seen_.size(); }

  /// Report that [peer] misbehaved (weight defaults to 1). Accumulates; once the
  /// total reaches the ban threshold the peer is banned. Returns true iff this
  /// call banned the peer.
  bool penalize(std::uint32_t peer, int weight = 1);
  bool banned(std::uint32_t peer) const { return banned_.count(peer) != 0; }
  int score(std::uint32_t peer) const;

 private:
  std::vector<Relay> flood(const Bytes& message, std::optional<std::uint32_t> except);
  bool mark_seen(const Hash& id);  // insert into the bounded window; true if newly seen
  void ban(std::uint32_t peer);

  std::uint32_t self_;
  Config cfg_;
  std::set<std::uint32_t> peers_;
  std::set<Hash> seen_;          // content hashes within the window
  std::deque<Hash> seen_order_;  // insertion order for FIFO eviction
  std::map<std::uint32_t, int> misbehavior_;           // peer -> accumulated penalty weight
  std::set<std::uint32_t> banned_;
  const consensus::CredentialGate* gate_ = nullptr;
};

}  // namespace aviary::p2p
