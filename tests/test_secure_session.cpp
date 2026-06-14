#include "blockchainer/secure_session.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using blockchainer::p2p::SecureSession;

namespace {
Bytes bytes(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}

// Establish a fully-open pair of sessions.
std::pair<SecureSession, SecureSession> handshake(const crypto::KeyPair& id) {
  auto [i, msg1] = SecureSession::begin_initiator(id.public_key);
  auto [r, msg2] = SecureSession::accept_responder(id.secret_key, msg1);
  i.complete_initiator(msg2);
  return {std::move(i), std::move(r)};
}
}  // namespace

int main() {
  init();

  auto id = SecureSession::generate_identity();

  std::printf("== handshake + bidirectional encryption ==\n");
  auto [I, R] = handshake(id);
  CHECK(I.open());
  CHECK(R.open());

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
    auto [I2, R2] = handshake(id);
    Bytes ct = I2.encrypt(bytes("tampered?"));
    ct[ct.size() / 2] ^= 0x01;
    CHECK_THROWS(R2.decrypt(ct));
  }

  std::printf("== out-of-order rejected (fresh session) ==\n");
  {
    auto [I3, R3] = handshake(id);
    Bytes f1 = I3.encrypt(bytes("one"));
    Bytes f2 = I3.encrypt(bytes("two"));
    CHECK_THROWS(R3.decrypt(f2));  // skipping f1 breaks the stream order
  }

  std::printf("== wrong identity key cannot decrypt ==\n");
  {
    auto wrong_id = SecureSession::generate_identity();
    auto [Ix, msg1] = SecureSession::begin_initiator(id.public_key);  // to id
    auto [Rbad, msg2] = SecureSession::accept_responder(wrong_id.secret_key, msg1);
    Bytes ct = Ix.encrypt(bytes("secret"));
    CHECK_THROWS(Rbad.decrypt(ct));
  }

  std::printf("== independent sessions derive independent keys ==\n");
  {
    auto [Ia, Ra] = handshake(id);
    auto [Ib, Rb] = handshake(id);
    Bytes ct = Ia.encrypt(bytes("for Ra only"));
    CHECK_THROWS(Rb.decrypt(ct));  // different ephemeral KEM → different keys
  }

  return bctest::summary();
}
