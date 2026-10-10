#include "aviary/tcp_transport.hpp"

#include "aviary/armored.hpp"
#include "aviary/noise_session.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "aviary/wire.hpp"

namespace aviary::p2p {

namespace {

using Clock = std::chrono::steady_clock;

// How long after arming before a consensus timeout fires. On localhost the happy
// path finishes via messages well before this; the timeout only matters when a
// proposer is slow or missing (or a link dropped and messages were lost), so a
// modest value keeps a real network live without making the tests slow.
constexpr int kTimeoutMs = 200;

// A node can't wait forever for a peer that will never connect (excluded by the
// allow-list, partitioned, or simply down). Once every expected peer is up we
// start immediately; otherwise we start anyway after this grace period and run
// consensus with whoever connected — which still finalizes if that is a quorum.
constexpr int kConnectGraceMs = 1500;

// Backoff between redial attempts to a peer whose connection dropped (or whose
// listener isn't up yet — initial dial and reconnect share one path).
constexpr int kRedialBackoffMs = 50;

// Cap on a connection's pending outbound bytes. A peer that stops reading (or
// reads too slowly) past this is disconnected rather than allowed to grow the
// queue without bound or wedge the reactor — gossip is best-effort, and a
// dropped dialer link redials anyway.
constexpr std::size_t kMaxOutboundBytes = 4u * 1024u * 1024u;

// How long to keep flushing after committing, so our final precommits reach a
// peer that is still one vote short before we tear the sockets down.
constexpr int kLingerMs = 300;

std::int64_t now_ms(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

void set_nonblocking(int fd) { ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

Bytes id_frame(std::uint32_t id) {
  return Bytes{static_cast<std::uint8_t>(id >> 24), static_cast<std::uint8_t>(id >> 16),
               static_cast<std::uint8_t>(id >> 8), static_cast<std::uint8_t>(id)};
}
std::uint32_t decode_id(const Bytes& b) {
  return (static_cast<std::uint32_t>(b[0]) << 24) | (static_cast<std::uint32_t>(b[1]) << 16) |
         (static_cast<std::uint32_t>(b[2]) << 8) | static_cast<std::uint32_t>(b[3]);
}

int make_listener(std::uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  int one = 1;
  ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
  if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 || ::listen(fd, 16) < 0) {
    ::close(fd);
    return -1;
  }
  // Non-blocking so the accept-drain loop stops at EAGAIN instead of blocking on
  // the next (absent) connection.
  set_nonblocking(fd);
  return fd;
}

int dial_once(const std::string& host, std::uint16_t port) {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) return -1;
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    ::close(fd);
    return -1;
  }
  // Blocking connect (instant on loopback: success or ECONNREFUSED), then
  // non-blocking for the reactor.
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return -1;
  }
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  set_nonblocking(fd);
  return fd;
}

// Per-connection state. `ready` means the link is usable for consensus: for a
// plaintext (or ratcheted) dialer that is immediate; for an accepter it waits
// for the hello; under the SIGMA tier the dialer also waits for msg2. Exactly
// one of sess/chan is set in secure mode (per the config tier).
struct Conn {
  bool dialer = false;
  std::uint32_t peer_id = 0;
  bool ready = false;
  // A Double-Ratchet responder cannot encrypt until it has decrypted its first
  // message ("no sending chain yet"). So a ratcheted accepter starts unable to
  // send; its outbound consensus is buffered here until the initiator's priming
  // frame arrives and opens its sending chain. Everyone else can send at once.
  bool can_send = true;
  std::vector<Bytes> send_backlog;
  Bytes in;                             // inbound bytes awaiting frame reassembly
  Bytes out;                            // outbound framed bytes awaiting POLLOUT flush
  std::unique_ptr<SecureSession> sess;  // kMutual tier
  std::unique_ptr<ArmoredChannel> chan; // kRatcheted tier
  std::unique_ptr<NoiseSession> noise;  // kNoiseXX tier
};

crypto::ByteView cv(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }

// The ratcheted tier wraps each plaintext in a 1-byte control tag: 0x01 = a data
// frame (consensus), 0x00 = a keepalive/priming frame (no payload). The tag lets
// the initiator prime the responder's sending chain without injecting a bogus
// consensus message — and cryptolib's AEAD rejects an empty plaintext, so the
// keepalive must carry at least this byte.
constexpr std::uint8_t kFrameData = 0x01;
constexpr std::uint8_t kFrameKeepalive = 0x00;

// Frame crypto that dispatches on the connection's tier (plaintext if neither).
Bytes conn_encrypt(Conn& c, const Bytes& pt) {
  if (c.chan) {
    Bytes framed;
    framed.reserve(pt.size() + 1);
    framed.push_back(kFrameData);
    framed.insert(framed.end(), pt.begin(), pt.end());
    return c.chan->encrypt(framed);
  }
  if (c.sess) return c.sess->encrypt(pt);
  if (c.noise) return c.noise->encrypt(pt);
  return pt;
}
// Returns the consensus payload, or empty for a ratcheted keepalive (which the
// caller treats as "received, not a message"). Throws on tamper.
Bytes conn_decrypt(Conn& c, const Bytes& frame) {
  if (c.chan) {
    Bytes plain = c.chan->decrypt(frame);
    if (plain.empty() || plain.front() == kFrameKeepalive) return {};
    return Bytes(plain.begin() + 1, plain.end());
  }
  if (c.noise) return c.noise->decrypt(frame);
  return c.sess->decrypt(frame);  // secure mode guarantees exactly one is set
}

}  // namespace

SecureConfig secure_config_from_records(TransportTier tier, SecureSession::Identity identity,
                                        crypto::KeyPair prekey, std::uint32_t self_id,
                                        const std::vector<PeerRecord>& records) {
  SecureConfig cfg;
  cfg.tier = tier;
  cfg.identity = std::move(identity);
  cfg.ratchet_prekey = std::move(prekey);
  for (const PeerRecord& r : records) {
    if (r.id == self_id) continue;
    cfg.peer_sig_pk[r.id] = r.sig_pk;                     // both tiers: allow-list / pin
    if (tier == TransportTier::kMutual) {
      cfg.peer_kem_pk[r.id] = r.kem_pk;                   // SIGMA dials to the KEM key
    } else if (tier == TransportTier::kNoiseXX) {
      // Noise XX learns the peer's static in-band; only the sig allow-list matters.
    } else if (!r.prekey.empty()) {
      cfg.peer_prekey[r.id] = r.prekey;                   // ratchet dials to the prekey
    }
  }
  return cfg;
}

bool run_tcp_bft_node(NetworkedNode& node, std::uint16_t listen_port,
                      const std::vector<PeerAddr>& peers, int deadline_ms, const SecureConfig* secure,
                      const ChaosConfig* chaos) {
  ::signal(SIGPIPE, SIG_IGN);  // never die because a peer vanished mid-send
  const Clock::time_point start = Clock::now();

  // Reverse allow-list: authenticated signing key → validator id.
  std::map<Bytes, std::uint32_t> id_by_sig;
  if (secure)
    for (const auto& [id, pk] : secure->peer_sig_pk) id_by_sig[pk] = id;
  const bool ratcheted = secure && secure->tier == TransportTier::kRatcheted;
  const bool noise = secure && secure->tier == TransportTier::kNoiseXX;
  // Noise XX static: the configured one, else fresh for this boot (the node's
  // identity is its hybrid sig key, proven per session — the static is not it).
  NoiseSession::Identity noise_id;
  if (noise) {
    noise_id.sig = secure->identity.sig;
    noise_id.noise_static = secure->noise_static.public_key.empty() ? crypto::x25519_keygen()
                                                                    : secure->noise_static;
  }

  int listener = make_listener(listen_port);
  if (listener < 0) return false;

  std::map<std::uint32_t, int> fd_by_peer;  // ready validator id → socket
  std::unordered_map<int, Conn> conns;      // socket → connection state
  const std::size_t expected = peers.size();

  // Initial dial and reconnect share one path: peers we are responsible for
  // dialing (higher id) sit in `redial_at` with the next attempt time; a dropped
  // dialer connection just re-enters the map with a backoff.
  std::map<std::uint32_t, PeerAddr> dial_targets;
  std::map<std::uint32_t, std::int64_t> redial_at;
  for (const auto& p : peers) {
    if (p.id > node.id()) {
      dial_targets[p.id] = p;
      redial_at[p.id] = 0;  // dial as soon as the reactor starts
    }
  }

  // Queue a frame for [fd] (4-byte BE length prefix + payload) and try to flush.
  // Returns false — and drops nothing itself — only on overflow; callers drop.
  auto flush = [&](int fd, Conn& c) -> bool {  // false = fatal socket error
    while (!c.out.empty()) {
      ssize_t w = ::send(fd, c.out.data(), c.out.size(), 0);
      if (w > 0) {
        c.out.erase(c.out.begin(), c.out.begin() + w);
      } else if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        return true;  // kernel buffer full — POLLOUT will resume this
      } else if (w < 0 && errno == EINTR) {
        continue;
      } else {
        return false;
      }
    }
    return true;
  };

  auto drop = [&](int fd) {
    auto it = conns.find(fd);
    if (it == conns.end()) return;
    Conn& c = it->second;
    // Only unmap the peer if this fd still IS its mapping — a redial may already
    // have installed a fresh socket that must not be clobbered by stale cleanup.
    if (c.ready) {
      auto m = fd_by_peer.find(c.peer_id);
      if (m != fd_by_peer.end() && m->second == fd) fd_by_peer.erase(m);
    }
    // A link we dial is re-dialed with backoff; an accepted link waits for the
    // remote dialer to come back.
    if (c.dialer && dial_targets.count(c.peer_id))
      redial_at[c.peer_id] = now_ms(start) + kRedialBackoffMs;
    ::close(fd);
    conns.erase(it);
  };

  auto queue_frame = [&](int fd, const Bytes& payload) {
    auto it = conns.find(fd);
    if (it == conns.end()) return;
    Conn& c = it->second;
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    if (c.out.size() + 4 + payload.size() > kMaxOutboundBytes) {
      drop(fd);  // backpressure overflow: the peer isn't reading — cut it off
      return;
    }
    c.out.push_back(static_cast<std::uint8_t>(len >> 24));
    c.out.push_back(static_cast<std::uint8_t>(len >> 16));
    c.out.push_back(static_cast<std::uint8_t>(len >> 8));
    c.out.push_back(static_cast<std::uint8_t>(len));
    c.out.insert(c.out.end(), payload.begin(), payload.end());
    if (!flush(fd, c)) drop(fd);
  };

  // Send a consensus payload on [fd], encrypting per the connection's tier. A
  // ratcheted responder that can't send yet (no sending chain) buffers it until
  // the initiator's priming frame opens the chain.
  auto send_payload = [&](int fd, const Bytes& plaintext) {
    auto it = conns.find(fd);
    if (it == conns.end()) return;
    Conn& c = it->second;
    if (!c.can_send) {
      c.send_backlog.push_back(plaintext);
      return;
    }
    queue_frame(fd, secure ? conn_encrypt(c, plaintext) : plaintext);
  };

  bool started = false;

  // A link just became usable for consensus. If consensus already started, the
  // peer may have missed messages (a redial after a drop, or a late joiner) —
  // gossip never re-floods what it already relayed, so without an explicit
  // replay a healed partition stays stalled. Replay our whole per-height log;
  // the peer's dedup discards what it already has.
  auto on_link_ready = [&](int fd, Conn& c) -> bool {  // false = conn dropped mid-replay
    fd_by_peer[c.peer_id] = fd;
    if (!started) return true;
    for (const Bytes& payload : node.replay_log()) {
      if (conns.find(fd) == conns.end()) return false;  // dropped (overflow) — caller must stop
      send_payload(fd, payload);
    }
    return conns.count(fd) != 0;
  };

  // Open (or reopen) the dialer-side link to [p]: connect, then either send the
  // plaintext id hello or start the secure handshake on the fresh socket.
  auto dial_peer = [&](const PeerAddr& p) {
    int fd = dial_once(p.host, p.port);
    if (fd < 0) {
      redial_at[p.id] = now_ms(start) + kRedialBackoffMs;
      return;
    }
    // Clear the schedule BEFORE queueing: if the very first flush fails, drop()
    // re-schedules the redial, and that must not be erased afterwards.
    redial_at.erase(p.id);
    Conn c;
    c.dialer = true;
    c.peer_id = p.id;
    if (ratcheted) {
      // Ratchet initiator: encapsulate to the peer's published prekey, pin its
      // identity. The channel opens immediately (no msg2 round), so — like the
      // plaintext dialer — it is ready at once; the armored hello goes out ahead
      // of any encrypted consensus frame.
      auto [chan, hello] = ArmoredChannel::initiate(cv(secure->peer_prekey.at(p.id)),
                                                    cv(secure->peer_sig_pk.at(p.id)),
                                                    secure->identity.sig);
      c.chan = std::make_unique<ArmoredChannel>(std::move(chan));
      c.ready = true;
      conns.emplace(fd, std::move(c));
      queue_frame(fd, hello);
      auto cit = conns.find(fd);
      if (cit != conns.end()) {
        // Prime the responder: a keepalive frame it can decrypt opens its
        // sending chain (a ratchet responder can't speak until it receives).
        queue_frame(fd, cit->second.chan->encrypt(Bytes{kFrameKeepalive}));
        on_link_ready(fd, cit->second);
      }
    } else if (noise) {
      // Noise XX initiator: msg0 out; ready once msg1 is consumed AND the
      // responder's identity proof verifies + pins.
      auto [s, msg0] = NoiseSession::begin_initiator(noise_id, secure->noise_prologue);
      c.noise = std::make_unique<NoiseSession>(std::move(s));
      conns.emplace(fd, std::move(c));
      queue_frame(fd, msg0);
    } else if (secure) {
      auto [s, msg1] = SecureSession::begin_initiator(secure->peer_kem_pk.at(p.id), secure->identity.sig);
      c.sess = std::make_unique<SecureSession>(std::move(s));
      conns.emplace(fd, std::move(c));
      queue_frame(fd, msg1);  // ready only after we consume the peer's msg2
    } else {
      c.ready = true;
      auto [it, inserted] = conns.emplace(fd, std::move(c));
      queue_frame(fd, id_frame(node.id()));  // announce who we are (before replay)
      auto cit = conns.find(fd);
      if (cit != conns.end()) on_link_ready(fd, cit->second);
    }
  };

  struct Timer {
    std::int64_t due;
    consensus::Step step;
    std::uint32_t round;
  };
  std::vector<Timer> timers;
  std::vector<std::pair<std::uint32_t, Bytes>> pending;  // frames held until we've started
  std::int64_t committed_at = -1;
  int chaos_frames = 0;
  bool chaos_done = false;

  auto dispatch = [&](const NetworkedNode::Outbound& out) {
    for (const auto& r : out.relays) {
      auto it = fd_by_peer.find(r.to);
      if (it == fd_by_peer.end()) continue;  // link down — gossip is best-effort
      const int fd = it->second;
      send_payload(fd, r.message);
    }
    for (const auto& t : out.timeouts) timers.push_back({now_ms(start) + kTimeoutMs, t.step, t.round});
  };

  while (now_ms(start) <= deadline_ms) {
    // (Re)dial any due targets — initial connections and dropped links alike.
    {
      const std::int64_t t = now_ms(start);
      std::vector<std::uint32_t> due;
      for (const auto& [id, at] : redial_at)
        if (at <= t) due.push_back(id);
      for (std::uint32_t id : due) dial_peer(dial_targets.at(id));
    }

    // Build the poll set: the listener + every live connection (write interest
    // only while output is pending).
    std::vector<pollfd> pfds;
    pfds.push_back({listener, POLLIN, 0});
    std::vector<int> order;
    for (const auto& [fd, c] : conns) {
      pfds.push_back({fd, static_cast<short>(POLLIN | (c.out.empty() ? 0 : POLLOUT)), 0});
      order.push_back(fd);
    }

    int rc = ::poll(pfds.data(), pfds.size(), /*tick=*/20);
    if (rc < 0 && errno != EINTR) break;

    // Accept new inbound connections (from lower-id peers, incl. redials).
    if (pfds[0].revents & POLLIN) {
      int cfd;
      while ((cfd = ::accept(listener, nullptr, nullptr)) >= 0) {
        int one = 1;
        ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        set_nonblocking(cfd);
        conns.emplace(cfd, Conn{});  // peer id + readiness learned from hello/handshake
      }
    }

    for (std::size_t i = 0; i < order.size(); ++i) {
      const int fd = order[i];
      auto cit = conns.find(fd);
      if (cit == conns.end()) continue;
      Conn& c = cit->second;

      // Resume a send the kernel previously couldn't take.
      if (pfds[i + 1].revents & POLLOUT) {
        if (!flush(fd, c)) {
          drop(fd);
          continue;
        }
      }

      if (!(pfds[i + 1].revents & (POLLIN | POLLHUP | POLLERR))) continue;

      // Drain everything readable (non-blocking socket → loop to EAGAIN).
      bool closed = false;
      std::uint8_t tmp[4096];
      for (;;) {
        ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
        if (r > 0) {
          c.in.insert(c.in.end(), tmp, tmp + r);
        } else if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
          break;
        } else if (r < 0 && errno == EINTR) {
          continue;
        } else {
          closed = true;  // orderly close or hard error
          break;
        }
      }
      if (closed) {
        drop(fd);
        continue;
      }

      bool dropped = false;
      while (!dropped && c.in.size() >= 4) {
        const std::uint32_t len = (static_cast<std::uint32_t>(c.in[0]) << 24) |
                                  (static_cast<std::uint32_t>(c.in[1]) << 16) |
                                  (static_cast<std::uint32_t>(c.in[2]) << 8) | static_cast<std::uint32_t>(c.in[3]);
        if (c.in.size() < 4u + len) break;
        Bytes frame(c.in.begin() + 4, c.in.begin() + 4 + len);
        c.in.erase(c.in.begin(), c.in.begin() + 4 + len);

        if (c.ready) {
          // Fault injection: sever this link once, after N frames from the peer.
          if (chaos && !chaos_done && c.peer_id == chaos->peer && ++chaos_frames >= chaos->after_frames) {
            chaos_done = true;
            drop(fd);
            dropped = true;
            break;
          }
          // A live consensus frame.
          if (secure) {
            try {
              Bytes pt = conn_decrypt(c, frame);
              // Receiving anything opens a ratcheted responder's sending chain;
              // flush whatever it buffered while it couldn't speak.
              if (!c.can_send) {
                c.can_send = true;
                for (const Bytes& b : c.send_backlog) queue_frame(fd, conn_encrypt(c, b));
                c.send_backlog.clear();
              }
              // An empty plaintext is a transport priming/keepalive, not a
              // consensus message — a real wire message is never zero-length.
              if (!pt.empty()) pending.emplace_back(c.peer_id, std::move(pt));
            } catch (...) {
              drop(fd);  // tamper / desync — cut the peer off (redial → fresh keys)
              dropped = true;
            }
          } else {
            pending.emplace_back(c.peer_id, std::move(frame));
          }
        } else if (noise) {
          // Noise XX: dialer = msg1 → (msg2 ‖ our proof), then the peer's proof;
          // accepter = msg0 → msg1, msg2 → our proof, then the peer's proof.
          // Nobody is `ready` (no consensus byte flows) until the peer's
          // PQ identity proof verified for THIS session and pinned/allow-listed.
          bool waiting = false;  // handshake step done, more frames needed
          try {
            if (c.dialer) {
              if (!c.noise->handshake_done()) {
                Bytes msg2 = c.noise->complete_initiator(frame);
                Bytes proof = c.noise->identity_proof();
                queue_frame(fd, msg2);
                if (conns.find(fd) == conns.end()) { dropped = true; continue; }
                queue_frame(fd, proof);
                if (conns.find(fd) == conns.end()) { dropped = true; continue; }
                waiting = true;
              } else {
                c.noise->verify_identity_proof(frame);
                if (c.noise->peer_identity() != secure->peer_sig_pk.at(c.peer_id))
                  throw std::runtime_error("authenticated key isn't the validator we dialed");
              }
            } else if (!c.noise) {
              auto [s, msg1] = NoiseSession::accept_responder(noise_id, frame, secure->noise_prologue);
              c.noise = std::make_unique<NoiseSession>(std::move(s));
              queue_frame(fd, msg1);
              if (conns.find(fd) == conns.end()) { dropped = true; continue; }
              waiting = true;
            } else if (!c.noise->handshake_done()) {
              c.noise->complete_responder(frame);
              Bytes proof = c.noise->identity_proof();
              queue_frame(fd, proof);
              if (conns.find(fd) == conns.end()) { dropped = true; continue; }
              waiting = true;
            } else {
              c.noise->verify_identity_proof(frame);
              auto found = id_by_sig.find(c.noise->peer_identity());
              if (found == id_by_sig.end()) throw std::runtime_error("unknown identity");
              c.peer_id = found->second;  // a redial replaces any stale mapping
            }
          } catch (...) {
            drop(fd);  // bad handshake / proof failed / not on the allow-list
            dropped = true;
            continue;
          }
          if (waiting) continue;
          c.ready = true;
          if (!on_link_ready(fd, c)) {
            dropped = true;
            continue;
          }
        } else if (ratcheted) {
          // Ratchet responder consuming the armored hello: accept, map the
          // authenticated initiator identity → validator id. No reply frame; the
          // channel is open and the next frames are encrypted consensus.
          try {
            c.chan = std::make_unique<ArmoredChannel>(
                ArmoredChannel::accept(frame, secure->ratchet_prekey, secure->identity.sig));
          } catch (...) {
            drop(fd);  // bad handshake / failed initiator auth
            dropped = true;
            continue;
          }
          auto found = id_by_sig.find(c.chan->peer_identity());
          if (found == id_by_sig.end()) {
            drop(fd);  // unknown identity — not on the allow-list
            dropped = true;
            continue;
          }
          c.peer_id = found->second;
          c.ready = true;
          c.can_send = false;  // responder can't encrypt until it decrypts once
          if (!on_link_ready(fd, c)) {
            dropped = true;
            continue;
          }
        } else if (secure && c.dialer) {
          // Initiator consuming msg2: complete + pin the peer's identity.
          try {
            c.sess->complete_initiator(frame);
          } catch (...) {
            drop(fd);
            dropped = true;
            continue;
          }
          if (c.sess->peer_identity() != secure->peer_sig_pk.at(c.peer_id)) {
            drop(fd);  // authenticated key isn't the validator we dialed
            dropped = true;
            continue;
          }
          c.ready = true;
          if (!on_link_ready(fd, c)) {
            dropped = true;
            continue;
          }
        } else if (secure) {
          // Responder consuming msg1: reply msg2, map identity → validator id.
          try {
            auto [s, msg2] = SecureSession::accept_responder(secure->identity, frame);
            c.sess = std::make_unique<SecureSession>(std::move(s));
            queue_frame(fd, msg2);
          } catch (...) {
            drop(fd);
            dropped = true;
            continue;
          }
          auto found = id_by_sig.find(c.sess->peer_identity());
          if (found == id_by_sig.end()) {
            drop(fd);  // unknown identity — not on the allow-list
            dropped = true;
            continue;
          }
          // A redial replaces any stale mapping for this peer.
          c.peer_id = found->second;
          c.ready = true;
          if (!on_link_ready(fd, c)) {
            dropped = true;
            continue;
          }
        } else {
          // Plaintext accepter: first frame is the dialer's id hello.
          if (frame.size() != 4) {
            drop(fd);
            dropped = true;
            continue;
          }
          c.peer_id = decode_id(frame);
          c.ready = true;
          if (!on_link_ready(fd, c)) {
            dropped = true;
            continue;
          }
        }
      }
    }

    // Begin consensus exactly once: as soon as every peer link is up, or after
    // the grace period with whoever connected (so an excluded/absent peer can't
    // stall an otherwise-quorate network).
    if (!started && (fd_by_peer.size() >= expected || now_ms(start) >= kConnectGraceMs)) {
      started = true;
      dispatch(node.start());
    }
    if (!started) continue;  // hold frames in `pending` until connections settle

    // Deliver received frames (a proposal that raced ahead of our StartRound was
    // buffered, so it is processed here, after start(), not before).
    for (auto& [from, payload] : pending) dispatch(node.on_gossip(from, payload));
    pending.clear();

    // Fire due timeouts (drop the ones we fire).
    const std::int64_t t = now_ms(start);
    std::vector<Timer> keep;
    for (const auto& tm : timers) {
      if (tm.due <= t)
        dispatch(node.on_timeout(tm.step, tm.round));
      else
        keep.push_back(tm);
    }
    timers.swap(keep);

    if (node.committed()) {
      // Keep the reactor running briefly after commit: our final precommits may
      // still sit in outbound queues (non-blocking sends), and a peer one vote
      // short needs them. POLLOUT flushing continues during the linger.
      if (committed_at < 0) committed_at = now_ms(start);
      if (now_ms(start) - committed_at >= kLingerMs) break;
    }
  }

  for (const auto& [fd, c] : conns) ::close(fd);
  ::close(listener);
  return node.committed();
}

}  // namespace aviary::p2p
