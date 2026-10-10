// Membership admission over a real socket: a gossip peer proves it holds an
// issuer-signed credential before it is admitted to the mesh
// (docs/design/CREDENTIAL_GATE.md §6). This drives the blocking
// challenge→present→admit round over a connected socketpair, verifier on one end
// and prover on the other (one thread each), and confirms the verdict for an
// honest member, a forged credential, a garbage responder, and a silent peer.
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>

#include "aviary/admission_socket.hpp"
#include "aviary/credential_gate.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

// Run a verifier on one end and a prover closure on the other; return whether the
// verifier admitted. The prover closure gets the peer fd and does whatever it
// likes (present honestly, send garbage, or stay silent).
template <class Prover>
bool run_exchange(const consensus::CredentialGate& gate, std::uint32_t epoch, std::uint64_t now,
                  Prover prover, int verifier_timeout_ms = 3000) {
  int sv[2];
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) != 0) return false;
  bool admitted = false;
  std::thread vt(
      [&] { admitted = p2p::admit_peer_over_socket(sv[0], gate, epoch, now, verifier_timeout_ms); });
  prover(sv[1]);
  vt.join();
  ::close(sv[0]);
  ::close(sv[1]);
  return admitted;
}
}  // namespace

int main() {
  init();

  // Issuer + policy + an honest member credential.
  auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x55)));
  consensus::CredentialPolicy policy;
  policy.chain_id = 3;
  policy.issuer_public_key = issuer.public_key;
  policy.header = bytes("aviary.credential.v1");
  policy.min_tier = 1;
  consensus::CredentialGate gate(policy);

  Bytes member_secret(32, 0x66);
  auto cred = consensus::issue_credential(policy, view(issuer.secret_key), view(member_secret),
                                          /*tier=*/2, /*not_after=*/4102444800ULL, /*issuer_epoch=*/1);
  const std::uint64_t now = 1700000000ULL;
  const std::uint32_t epoch = 4;

  // ── Honest member is admitted ──────────────────────────────────────────────
  bool ok = run_exchange(gate, epoch, now, [&](int fd) {
    CHECK(p2p::present_over_socket(fd, policy, cred, 3000));
  });
  CHECK_MSG(ok, "an honest member is admitted over the socket");

  // ── Unlinkability across two real connections ──────────────────────────────
  bool ok2 = run_exchange(gate, epoch, now,
                          [&](int fd) { p2p::present_over_socket(fd, policy, cred, 3000); });
  CHECK_MSG(ok2, "the same member is admitted again on a fresh connection");

  // ── A forged credential (wrong issuer) is refused ──────────────────────────
  auto rogue = crypto::bbs_keygen(view(Bytes(32, 0xAA)));
  auto forged = consensus::issue_credential(policy, view(rogue.secret_key), view(member_secret), 2,
                                            4102444800ULL, 1);
  bool bad = run_exchange(gate, epoch, now,
                          [&](int fd) { p2p::present_over_socket(fd, policy, forged, 3000); });
  CHECK_MSG(!bad, "a forged credential is refused over the socket");

  // ── A garbage responder is refused (verifier stays total) ──────────────────
  bool garbage = run_exchange(gate, epoch, now, [&](int fd) {
    // Read the challenge, then reply with junk instead of a presentation.
    std::uint8_t hdr[4];
    (void)::read(fd, hdr, 4);
    std::size_t clen = (std::size_t(hdr[0]) << 24) | (std::size_t(hdr[1]) << 16) |
                       (std::size_t(hdr[2]) << 8) | hdr[3];
    Bytes drain(clen);
    (void)::read(fd, drain.data(), clen);
    const std::uint8_t junk[7] = {0, 0, 0, 3, 'x', 'y', 'z'};  // len=3, "xyz"
    (void)::write(fd, junk, sizeof(junk));
  });
  CHECK_MSG(!garbage, "a garbage responder is refused");

  // ── A silent peer times out to a refusal, not a hang ───────────────────────
  bool silent = run_exchange(
      gate, epoch, now,
      [&](int fd) {
        (void)fd;  // read nothing, send nothing — the verifier's recv must time out
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
      },
      /*verifier_timeout_ms=*/200);
  CHECK_MSG(!silent, "a silent peer is refused (verifier times out)");

  return bctest::summary();
}
