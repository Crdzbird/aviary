#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "blockchainer/networked_node.hpp"

/// Real-socket transport for a [NetworkedNode] — the last mile that turns the
/// pure in/out validator into one that runs over the network.
///
/// [NetworkedNode] is deliberately clockless and socketless: it returns the
/// gossip relays to send and the timeouts to arm. This module is the adapter
/// that drives it over TCP with a single-threaded reactor (poll(2)): no locks,
/// because exactly one thread ever touches the node. Messages are length-
/// prefixed frames; on connect the dialer sends a 4-byte node-id hello so the
/// accepter can map the socket to a validator index. Consensus timeouts are
/// armed against a steady clock and fired when due.
///
/// Topology is formed deterministically: for an edge, the lower id dials the
/// higher id (so each node dials its higher-id peers and accepts its lower-id
/// peers). The node waits until every expected peer connection is up, then
/// begins consensus — so the round-0 proposer never broadcasts into the void.
namespace blockchainer::p2p {

/// Where to reach a peer validator.
struct PeerAddr {
  std::uint32_t id;
  std::string host;
  std::uint16_t port;
};

/// Drive [node] over TCP: listen on [listen_port], connect the topology implied
/// by [peers], then run the reactor until the node commits or [deadline_ms]
/// elapses. Blocking — run one per thread. Returns true iff the node committed.
bool run_tcp_bft_node(NetworkedNode& node, std::uint16_t listen_port,
                      const std::vector<PeerAddr>& peers, int deadline_ms = 8000);

}  // namespace blockchainer::p2p
