#include "blockchainer/tcp_transport.hpp"

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
#include <unordered_map>
#include <vector>

#include "blockchainer/wire.hpp"

namespace blockchainer::p2p {

namespace {

using Clock = std::chrono::steady_clock;

// How long after arming before a consensus timeout fires. On localhost the happy
// path finishes via messages well before this; the timeout only matters when a
// proposer is slow or missing, so a modest value keeps a real network live
// without making the test slow.
constexpr int kTimeoutMs = 200;

// A node can't wait forever for a peer that will never connect (excluded by the
// allow-list, partitioned, or simply down). Once every expected peer is up we
// start immediately; otherwise we start anyway after this grace period and run
// consensus with whoever connected — which still finalizes if that is a quorum.
constexpr int kConnectGraceMs = 1500;

std::int64_t now_ms(Clock::time_point start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
}

// Write exactly n bytes (blocking socket, but tolerate short writes / EINTR).
bool write_all(int fd, const std::uint8_t* p, std::size_t n) {
  std::size_t off = 0;
  while (off < n) {
    ssize_t w = ::send(fd, p + off, n - off, 0);
    if (w > 0) {
      off += static_cast<std::size_t>(w);
    } else if (w < 0 && (errno == EINTR || errno == EAGAIN)) {
      continue;
    } else {
      return false;  // peer gone — gossip is best-effort, caller drops it
    }
  }
  return true;
}

// A 4-byte big-endian length prefix followed by the payload. Every message on
// the wire — the id hello, the handshake, and every (encrypted) consensus frame
// — is one of these, so reassembly of partial reads is uniform.
bool send_frame(int fd, const Bytes& payload) {
  std::uint8_t hdr[4];
  const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
  hdr[0] = static_cast<std::uint8_t>(len >> 24);
  hdr[1] = static_cast<std::uint8_t>(len >> 16);
  hdr[2] = static_cast<std::uint8_t>(len >> 8);
  hdr[3] = static_cast<std::uint8_t>(len);
  if (!write_all(fd, hdr, 4)) return false;
  return payload.empty() || write_all(fd, payload.data(), payload.size());
}

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
  ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
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
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return -1;
  }
  int one = 1;
  ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
  return fd;
}

// Per-connection state. `ready` means the link is usable for consensus: for a
// plaintext dialer that is immediate; for an accepter it waits for the id hello;
// under [SecureConfig] it waits for the handshake to complete.
struct Conn {
  bool dialer = false;
  std::uint32_t peer_id = 0;
  bool ready = false;
  Bytes buf;
  std::unique_ptr<SecureSession> sess;  // secure mode only
};

}  // namespace

bool run_tcp_bft_node(NetworkedNode& node, std::uint16_t listen_port,
                      const std::vector<PeerAddr>& peers, int deadline_ms, const SecureConfig* secure) {
  ::signal(SIGPIPE, SIG_IGN);  // never die because a peer vanished mid-send
  const Clock::time_point start = Clock::now();

  // Reverse allow-list: authenticated signing key → validator id.
  std::map<Bytes, std::uint32_t> id_by_sig;
  if (secure)
    for (const auto& [id, pk] : secure->peer_sig_pk) id_by_sig[pk] = id;

  int listener = make_listener(listen_port);
  if (listener < 0) return false;

  std::map<std::uint32_t, int> fd_by_peer;  // ready validator id → socket
  std::unordered_map<int, Conn> conns;      // socket → receive state
  const std::size_t expected = peers.size();

  auto drop = [&](int fd, Conn& c) {
    if (c.ready) fd_by_peer.erase(c.peer_id);
    ::close(fd);
    conns.erase(fd);
  };

  // Dial every higher-id peer (retry until it is listening or we run out of time).
  for (const auto& p : peers) {
    if (p.id <= node.id()) continue;  // lower id dials us; we accept below
    int fd = -1;
    while ((fd = dial_once(p.host, p.port)) < 0) {
      if (now_ms(start) > deadline_ms) {
        ::close(listener);
        return false;
      }
      struct timespec ts{0, 10 * 1000 * 1000};  // 10ms backoff
      ::nanosleep(&ts, nullptr);
    }
    Conn c;
    c.dialer = true;
    c.peer_id = p.id;
    if (secure) {
      // Initiator: encapsulate to the peer's KEM key, sign the transcript.
      auto [s, msg1] = SecureSession::begin_initiator(secure->peer_kem_pk.at(p.id), secure->identity.sig);
      c.sess = std::make_unique<SecureSession>(std::move(s));
      send_frame(fd, msg1);  // ready only after we consume the peer's msg2
    } else {
      send_frame(fd, id_frame(node.id()));  // announce who we are
      c.ready = true;
      fd_by_peer[p.id] = fd;
    }
    conns.emplace(fd, std::move(c));
  }

  struct Timer {
    std::int64_t due;
    consensus::Step step;
    std::uint32_t round;
  };
  std::vector<Timer> timers;
  std::vector<std::pair<std::uint32_t, Bytes>> pending;  // frames held until we've started
  bool started = false;

  auto dispatch = [&](const NetworkedNode::Outbound& out) {
    for (const auto& r : out.relays) {
      auto it = fd_by_peer.find(r.to);
      if (it == fd_by_peer.end()) continue;
      const int fd = it->second;
      if (secure) {
        auto cit = conns.find(fd);
        if (cit != conns.end() && cit->second.sess) send_frame(fd, cit->second.sess->encrypt(r.message));
      } else {
        send_frame(fd, r.message);
      }
    }
    for (const auto& t : out.timeouts) timers.push_back({now_ms(start) + kTimeoutMs, t.step, t.round});
  };

  while (now_ms(start) <= deadline_ms) {
    std::vector<pollfd> pfds;
    pfds.push_back({listener, POLLIN, 0});
    std::vector<int> order;
    for (const auto& [fd, c] : conns) {
      pfds.push_back({fd, POLLIN, 0});
      order.push_back(fd);
    }

    int rc = ::poll(pfds.data(), pfds.size(), /*tick=*/20);
    if (rc < 0 && errno != EINTR) break;

    // Accept new inbound connections (from lower-id peers).
    if (pfds[0].revents & POLLIN) {
      int cfd;
      while ((cfd = ::accept(listener, nullptr, nullptr)) >= 0) {
        int one = 1;
        ::setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        conns.emplace(cfd, Conn{});  // peer id + readiness learned from hello/handshake
      }
    }

    // Drain readable connections into complete frames.
    for (std::size_t i = 0; i < order.size(); ++i) {
      const int fd = order[i];
      if (!(pfds[i + 1].revents & (POLLIN | POLLHUP | POLLERR))) continue;
      auto cit = conns.find(fd);
      if (cit == conns.end()) continue;
      Conn& c = cit->second;

      std::uint8_t tmp[4096];
      ssize_t r = ::recv(fd, tmp, sizeof(tmp), 0);
      if (r > 0) {
        c.buf.insert(c.buf.end(), tmp, tmp + r);
      } else if (r == 0 || (r < 0 && errno != EAGAIN && errno != EINTR)) {
        drop(fd, c);
        continue;
      }

      bool dropped = false;
      while (!dropped && c.buf.size() >= 4) {
        const std::uint32_t len = (static_cast<std::uint32_t>(c.buf[0]) << 24) |
                                  (static_cast<std::uint32_t>(c.buf[1]) << 16) |
                                  (static_cast<std::uint32_t>(c.buf[2]) << 8) | static_cast<std::uint32_t>(c.buf[3]);
        if (c.buf.size() < 4u + len) break;
        Bytes frame(c.buf.begin() + 4, c.buf.begin() + 4 + len);
        c.buf.erase(c.buf.begin(), c.buf.begin() + 4 + len);

        if (c.ready) {
          // A live consensus frame.
          if (secure) {
            try {
              pending.emplace_back(c.peer_id, c.sess->decrypt(frame));
            } catch (...) {
              drop(fd, c);  // tamper / desync — cut the peer off
              dropped = true;
            }
          } else {
            pending.emplace_back(c.peer_id, std::move(frame));
          }
        } else if (secure && c.dialer) {
          // Initiator consuming msg2: complete + pin the peer's identity.
          try {
            c.sess->complete_initiator(frame);
          } catch (...) {
            drop(fd, c);
            dropped = true;
            continue;
          }
          if (c.sess->peer_identity() != secure->peer_sig_pk.at(c.peer_id)) {
            drop(fd, c);  // authenticated key isn't the validator we dialed
            dropped = true;
            continue;
          }
          c.ready = true;
          fd_by_peer[c.peer_id] = fd;
        } else if (secure) {
          // Responder consuming msg1: reply msg2, map identity → validator id.
          try {
            auto [s, msg2] = SecureSession::accept_responder(secure->identity, frame);
            c.sess = std::make_unique<SecureSession>(std::move(s));
            send_frame(fd, msg2);
          } catch (...) {
            drop(fd, c);
            dropped = true;
            continue;
          }
          auto found = id_by_sig.find(c.sess->peer_identity());
          if (found == id_by_sig.end()) {
            drop(fd, c);  // unknown identity — not on the allow-list
            dropped = true;
            continue;
          }
          c.peer_id = found->second;
          c.ready = true;
          fd_by_peer[c.peer_id] = fd;
        } else {
          // Plaintext accepter: first frame is the dialer's id hello.
          c.peer_id = decode_id(frame);
          c.ready = true;
          fd_by_peer[c.peer_id] = fd;
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
      // Our final precommits were written to the kernel by dispatch() above; the
      // OS flushes them to peers after close(). Linger briefly so a peer still
      // one vote short receives ours before we tear the socket down.
      struct timespec ts{0, 300 * 1000 * 1000};  // 300ms
      ::nanosleep(&ts, nullptr);
      break;
    }
  }

  for (const auto& [fd, c] : conns) ::close(fd);
  ::close(listener);
  return node.committed();
}

}  // namespace blockchainer::p2p
