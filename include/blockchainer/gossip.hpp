#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// Gossip overlay (ARCHITECTURE §9). The wire protocol and secure sessions carry
/// messages between two peers; this is the mesh on top: each node dedups
/// messages by content hash and floods a new one to its peers (except whoever
/// sent it), so a message reaches every connected node exactly once, without
/// loops or floods, over an arbitrary (partial) topology.
///
/// Transport-agnostic and deterministic: the overlay returns the relays it wants
/// sent; the caller wires them to a real socket or the in-memory transport.
namespace blockchainer::p2p {

class GossipOverlay {
 public:
  explicit GossipOverlay(std::uint32_t self) : self_(self) {}

  void add_peer(std::uint32_t peer) {
    if (peer != self_) peers_.insert(peer);
  }
  std::uint32_t self() const { return self_; }
  const std::set<std::uint32_t>& peers() const { return peers_; }

  /// A message to relay to a specific peer.
  struct Relay {
    std::uint32_t to;
    Bytes message;
  };

  /// Originate [message] from this node → relay it to every peer.
  std::vector<Relay> originate(const Bytes& message);

  /// Handle [message] received from peer [from]. If it is new, mark it seen and
  /// relay it to every peer except [from] (returned); if already seen, return
  /// nothing (dedup — this is what stops loops and floods).
  std::vector<Relay> on_receive(std::uint32_t from, const Bytes& message);

  /// Whether this node has already processed [message].
  bool seen(const Bytes& message) const;
  std::size_t seen_count() const { return seen_.size(); }

 private:
  std::vector<Relay> flood(const Bytes& message, std::optional<std::uint32_t> except);

  std::uint32_t self_;
  std::set<std::uint32_t> peers_;
  std::set<Hash> seen_;  // content hashes of processed messages
};

}  // namespace blockchainer::p2p
