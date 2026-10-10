#include "aviary/secure_session.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using aviary::p2p::SecureSession;

namespace {
Bytes bytes(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}

// Establish a fully-open, mutually authenticated pair of sessions.
std::pair<SecureSession, SecureSession> handshake(const SecureSession::Identity& responder,
                                                  const crypto::KeyPair& initiator_sig) {
  auto [i, msg1] = SecureSession::begin_initiator(responder.kem.public_key, initiator_sig);
  auto [r, msg2] = SecureSession::accept_responder(responder, msg1);
  i.complete_initiator(msg2);
  return {std::move(i), std::move(r)};
}
}  // namespace

int main() {
  init();

  auto responder = SecureSession::generate_identity();
  auto initiator = SecureSession::generate_identity();

  std::printf("== handshake + bidirectional encryption ==\n");
  auto [I, R] = handshake(responder, initiator.sig);
  CHECK(I.open());
  CHECK(R.open());

  std::printf("== mutual authentication: each side learns the peer's real key ==\n");
  CHECK(R.peer_identity() == initiator.sig.public_key);  // responder authenticated the initiator
  CHECK(I.peer_identity() == responder.sig.public_key);  // initiator authenticated the responder

  Bytes m1 = bytes("hello responder");
  Bytes c1 = I.encrypt(m1);
  CHECK(c1 != m1);              // actually encrypted
  CHECK(R.decrypt(c1) == m1);  // and recovered

  Bytes m2 = bytes("hello initiator");
  CHECK(I.decrypt(R.encrypt(m2)) == m2);  // reverse direction

  std::printf("== in-order multi-frame ==\n");
  Bytes fa = I.encrypt(bytes("frame-a"));
  Bytes fb = I.encrypt(bytes("frame-b"));
  CHECK(R.decrypt(fa) == bytes("frame-a"));
  CHECK(R.decrypt(fb) == bytes("frame-b"));

  std::printf("== tamper detection (fresh session) ==\n");
  {
    auto [I2, R2] = handshake(responder, initiator.sig);
    Bytes ct = I2.encrypt(bytes("tampered?"));
    ct[ct.size() / 2] ^= 0x01;
    CHECK_THROWS(R2.decrypt(ct));
  }

  std::printf("== out-of-order rejected (fresh session) ==\n");
  {
    auto [I3, R3] = handshake(responder, initiator.sig);
    Bytes f1 = I3.encrypt(bytes("one"));
    Bytes f2 = I3.encrypt(bytes("two"));
    CHECK_THROWS(R3.decrypt(f2));  // skipping f1 breaks the stream order
  }

  std::printf("== wrong responder KEM key aborts the handshake ==\n");
  {
    auto wrong = SecureSession::generate_identity();
    auto [Ix, msg1] = SecureSession::begin_initiator(responder.kem.public_key, initiator.sig);
    // Responder uses the wrong KEM secret → different shared secret → the
    // initiator's transcript signature fails to verify → abort (no bad channel).
    CHECK_THROWS(SecureSession::accept_responder(wrong, msg1));
  }

  std::printf("== forged initiator signature is rejected (MITM cannot impersonate) ==\n");
  {
    auto [Iy, msg1] = SecureSession::begin_initiator(responder.kem.public_key, initiator.sig);
    Bytes forged = msg1;
    forged[forged.size() - 1] ^= 0x01;  // flip a byte of sig_I
    CHECK_THROWS(SecureSession::accept_responder(responder, forged));
  }

  std::printf("== forged responder signature is rejected ==\n");
  {
    auto [Iz, msg1] = SecureSession::begin_initiator(responder.kem.public_key, initiator.sig);
    auto [Rz, msg2] = SecureSession::accept_responder(responder, msg1);
    Bytes forged = msg2;
    forged[forged.size() - 1] ^= 0x01;  // flip a byte of sig_R
    CHECK_THROWS(Iz.complete_initiator(forged));
  }

  std::printf("== an impostor initiator authenticates as ITS OWN key, not the victim's ==\n");
  {
    // A MITM presenting its own identity completes a handshake, but the
    // responder sees the MITM's key — so allow-list pinning rejects it.
    auto mitm = SecureSession::generate_identity();
    auto [Im, msg1] = SecureSession::begin_initiator(responder.kem.public_key, mitm.sig);
    auto [Rm, msg2] = SecureSession::accept_responder(responder, msg1);
    CHECK(Rm.peer_identity() == mitm.sig.public_key);
    CHECK(Rm.peer_identity() != initiator.sig.public_key);
  }

  std::printf("== independent sessions derive independent keys ==\n");
  {
    auto [Ia, Ra] = handshake(responder, initiator.sig);
    auto [Ib, Rb] = handshake(responder, initiator.sig);
    Bytes ct = Ia.encrypt(bytes("for Ra only"));
    CHECK_THROWS(Rb.decrypt(ct));  // different ephemeral KEM → different keys
  }

  return bctest::summary();
}
