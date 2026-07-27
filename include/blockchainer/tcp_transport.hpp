#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "blockchainer/networked_node.hpp"
#include "blockchainer/peer_book.hpp"
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

/// Opt-in encryption + authentication for the transport. Every connection runs
/// a mutually-authenticated handshake (dialer = initiator, accepter = responder)
/// before any consensus byte flows, and every frame is then encrypted; an
/// unknown identity is dropped (an allow-list). The [tier] picks which channel:
///
///   - `kMutual` — the SIGMA [SecureSession]: one-shot hybrid-KEM + signed
///     transcript. Confidential, mutually authenticated, PQ.
///   - `kRatcheted` — the [ArmoredChannel] hybrid-KEM Double Ratchet: adds
///     per-message forward secrecy + post-compromise healing on long-lived
///     links. The dialer encapsulates to the peer's published ratchet prekey.
///
/// The tier is what an `AssuranceProfile` selects; `secure_config_for` builds
/// the matching config from a node's identity + its peers' records.
enum class TransportTier { kMutual, kRatcheted };

struct SecureConfig {
  TransportTier tier = TransportTier::kMutual;
  SecureSession::Identity identity;              ///< this node's long-term identity (its .sig authenticates)
  std::map<std::uint32_t, Bytes> peer_sig_pk;   ///< peer id → signing public key (pin / allow-list; both tiers)

  // kMutual: the SIGMA KEM keys.
  std::map<std::uint32_t, Bytes> peer_kem_pk;   ///< peer id → hybrid-KEM public key (to dial)

  // kRatcheted: the ratchet prekeys.
  crypto::KeyPair ratchet_prekey;               ///< this node's ratchet prekey (to accept)
  std::map<std::uint32_t, Bytes> peer_prekey;   ///< peer id → ratchet prekey public key (to dial)
};

/// Assemble the transport [SecureConfig] for validator [self_id] at [tier] from
/// its long-term [identity], its ratchet [prekey], and the authenticated peer
/// [records] (each supplying the peer's KEM key, signing key, and ratchet
/// prekey). This is the boot-time bridge from an `AssuranceProfile` — a node
/// maps `profile.transport()` to a [TransportTier] and passes its address book:
///
///   AssuranceProfile::Transport::kMutual    → TransportTier::kMutual
///   AssuranceProfile::Transport::kRatcheted → TransportTier::kRatcheted
///
/// Peers with `id == self_id` are skipped; a record missing the material the
/// tier needs simply contributes nothing for that peer.
SecureConfig secure_config_from_records(TransportTier tier, SecureSession::Identity identity,
                                        crypto::KeyPair prekey, std::uint32_t self_id,
                                        const std::vector<PeerRecord>& records);

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
