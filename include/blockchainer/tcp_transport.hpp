#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "blockchainer/networked_node.hpp"
#include "blockchainer/secure_session.hpp"

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

/// Opt-in encryption + authentication for the transport. When supplied, every
/// connection runs the [SecureSession] handshake (dialer = initiator, accepter =
/// responder) before any consensus byte flows, and every frame is then encrypted
/// and integrity-protected. The dialer encapsulates to the peer's known KEM key
/// and pins the peer's authenticated signing key; the accepter maps the
/// authenticated signing key back to a validator id (an allow-list — an unknown
/// identity is dropped). So consensus runs over a confidential, mutually-
/// authenticated, post-quantum-hybrid channel with no plaintext on the wire.
struct SecureConfig {
  SecureSession::Identity identity;              ///< this node's long-term identity
  std::map<std::uint32_t, Bytes> peer_kem_pk;   ///< peer id → KEM public key (to dial)
  std::map<std::uint32_t, Bytes> peer_sig_pk;   ///< peer id → signing public key (to pin / allow-list)
};

/// Fault injection for tests: after [after_frames] complete frames have arrived
/// from [peer], the node closes that connection once — simulating a mid-consensus
/// link drop. The lifecycle machinery (dialer-side redial with backoff) must then
/// heal the link for consensus to finish.
struct ChaosConfig {
  std::uint32_t peer = 0;
  int after_frames = 1;
};

/// Drive [node] over TCP: listen on [listen_port], connect the topology implied
/// by [peers], then run the reactor until the node commits or [deadline_ms]
/// elapses. If [secure] is non-null, the channel is encrypted + mutually
/// authenticated (see [SecureConfig]); otherwise it is plaintext. Blocking — run
/// one per thread. Returns true iff the node committed.
///
/// Lifecycle: sockets are non-blocking; sends go through bounded per-connection
/// outbound queues flushed on POLLOUT (a peer that stops reading past the cap is
/// disconnected, not allowed to wedge the reactor). A dropped connection to a
/// peer this node dials is automatically re-dialed with backoff — the handshake
/// (id hello / secure session) reruns on the fresh socket. Messages lost on the
/// dead link are NOT replayed; consensus recovers via its own round changes.
bool run_tcp_bft_node(NetworkedNode& node, std::uint16_t listen_port,
                      const std::vector<PeerAddr>& peers, int deadline_ms = 8000,
                      const SecureConfig* secure = nullptr, const ChaosConfig* chaos = nullptr);

}  // namespace blockchainer::p2p
