#include "aviary/admission_socket.hpp"

#include <poll.h>
#include <sys/socket.h>

#include <cerrno>
#include <cstddef>
#include <optional>

#include "aviary/admission.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

namespace aviary::p2p {

namespace {
crypto::ByteView cv(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }

// A presentation is a few hundred bytes; cap the frame well above that so a
// malformed length can't drive a huge allocation.
constexpr std::size_t kMaxAdmissionFrame = 64u * 1024u;

bool send_all(int fd, const std::uint8_t* p, std::size_t n) {
  std::size_t off = 0;
  while (off < n) {
    ssize_t w = ::send(fd, p + off, n - off, 0);
    if (w > 0) {
      off += static_cast<std::size_t>(w);
    } else if (w < 0 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}

bool send_frame(int fd, const Bytes& b) {
  const std::uint8_t hdr[4] = {
      static_cast<std::uint8_t>(b.size() >> 24), static_cast<std::uint8_t>(b.size() >> 16),
      static_cast<std::uint8_t>(b.size() >> 8), static_cast<std::uint8_t>(b.size())};
  return send_all(fd, hdr, 4) && (b.empty() || send_all(fd, b.data(), b.size()));
}

bool recv_all(int fd, std::uint8_t* p, std::size_t n, int timeout_ms) {
  std::size_t off = 0;
  while (off < n) {
    struct pollfd pfd { fd, POLLIN, 0 };
    int rc = ::poll(&pfd, 1, timeout_ms);
    if (rc <= 0) return false;  // timeout or poll error
    ssize_t r = ::recv(fd, p + off, n - off, 0);
    if (r > 0) {
      off += static_cast<std::size_t>(r);
    } else if (r < 0 && errno == EINTR) {
      continue;
    } else {
      return false;  // peer closed or hard error
    }
  }
  return true;
}

std::optional<Bytes> recv_frame(int fd, int timeout_ms) {
  std::uint8_t hdr[4];
  if (!recv_all(fd, hdr, 4, timeout_ms)) return std::nullopt;
  const std::size_t len = (static_cast<std::size_t>(hdr[0]) << 24) |
                          (static_cast<std::size_t>(hdr[1]) << 16) |
                          (static_cast<std::size_t>(hdr[2]) << 8) | static_cast<std::size_t>(hdr[3]);
  if (len > kMaxAdmissionFrame) return std::nullopt;
  Bytes b(len);
  if (len > 0 && !recv_all(fd, b.data(), len, timeout_ms)) return std::nullopt;
  return b;
}
}  // namespace

bool admit_peer_over_socket(int fd, const consensus::CredentialGate& gate, std::uint32_t epoch,
                            std::uint64_t now, int timeout_ms) {
  AdmissionChallenge ch = make_admission_challenge(epoch);
  if (!send_frame(fd, ch.wire)) return false;
  std::optional<Bytes> reply = recv_frame(fd, timeout_ms);
  if (!reply) return false;
  return admission_granted(gate, epoch, cv(ch.nonce), cv(*reply), now);
}

bool present_over_socket(int fd, const consensus::CredentialPolicy& policy,
                         const consensus::Credential& credential, int timeout_ms) {
  std::optional<Bytes> challenge = recv_frame(fd, timeout_ms);
  if (!challenge) return false;
  Bytes reply;
  try {
    reply = answer_admission_challenge(policy, credential, cv(*challenge));
  } catch (...) {
    return false;
  }
  return send_frame(fd, reply);
}

}  // namespace aviary::p2p
