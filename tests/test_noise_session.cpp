// Noise XX tier: standards-based channel + PQ identity proof. Proves the
// round trip, fail-closed ordering (no traffic before the peer's proof), tamper
// / reorder rejection, prologue agreement, and — the point of the proof — that a
// MITM holding both legs' Noise keys still cannot relay an honest party's
// identity into the other session (the proof is bound to the handshake hash).
#include <cstdio>

#include "aviary/noise_session.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using aviary::p2p::NoiseSession;

namespace {
Bytes bytes(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}
crypto::ByteView v(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }

// Full handshake + mutual proofs → two open sessions.
std::pair<NoiseSession, NoiseSession> connect(const NoiseSession::Identity& ini,
                                              const NoiseSession::Identity& res, const Bytes& prologue = {}) {
  auto [I, msg0] = NoiseSession::begin_initiator(ini, prologue);
  auto [R, msg1] = NoiseSession::accept_responder(res, msg0, prologue);
  Bytes msg2 = I.complete_initiator(msg1);
  R.complete_responder(msg2);
  Bytes pi = I.identity_proof();
  Bytes pr = R.identity_proof();
  R.verify_identity_proof(pi);
  I.verify_identity_proof(pr);
  return {std::move(I), std::move(R)};
}
}  // namespace

int main() {
  init();
  auto alice = NoiseSession::generate_identity();
  auto bob = NoiseSession::generate_identity();
  CHECK(alice.noise_static.public_key.size() == 32 && alice.noise_static.secret_key.size() == 32);

  std::printf("== handshake, proofs, bidirectional transport ==\n");
  {
    auto [I, R] = connect(alice, bob);
    CHECK(I.open() && R.open());
    CHECK(I.handshake_hash() == R.handshake_hash());          // channel binding agreed
    CHECK(R.peer_identity() == alice.sig.public_key);          // responder authenticated alice (PQ key)
    CHECK(I.peer_identity() == bob.sig.public_key);
    CHECK(R.peer_static() == alice.noise_static.public_key);   // Noise-level statics learned in-band
    CHECK(I.peer_static() == bob.noise_static.public_key);
    Bytes m = bytes("hello bob");
    Bytes c = I.encrypt(m);
    CHECK(c != m && R.decrypt(c) == m);
    CHECK(I.decrypt(R.encrypt(bytes("hello alice"))) == bytes("hello alice"));
    Bytes a = I.encrypt(bytes("a")), b = I.encrypt(bytes("b"));
    CHECK(R.decrypt(a) == bytes("a") && R.decrypt(b) == bytes("b"));
  }

  std::printf("== fail-closed: no transport before the peer's proof verified ==\n");
  {
    auto [I, msg0] = NoiseSession::begin_initiator(alice);
    auto [R, msg1] = NoiseSession::accept_responder(bob, msg0);
    CHECK_THROWS(I.identity_proof());                // handshake not done
    Bytes msg2 = I.complete_initiator(msg1);
    CHECK(I.handshake_done() && !I.open());          // keys split, peer unproven
    CHECK_THROWS(I.encrypt(bytes("too early")));
    R.complete_responder(msg2);
    CHECK_THROWS(R.decrypt(bytes("too early")));
    Bytes pi = I.identity_proof();
    CHECK_THROWS(I.identity_proof());                // once only
    R.verify_identity_proof(pi);
    CHECK(R.open() && !I.open());
    I.verify_identity_proof(R.identity_proof());
    CHECK(I.open());
    CHECK_THROWS(I.verify_identity_proof(bytes("again")));  // already authenticated
  }

  std::printf("== tamper + reorder rejected ==\n");
  {
    auto [I, R] = connect(alice, bob);
    Bytes ct = I.encrypt(bytes("tampered?"));
    ct[ct.size() / 2] ^= 0x01;
    CHECK_THROWS(R.decrypt(ct));
  }
  {
    auto [I, R] = connect(alice, bob);
    Bytes f1 = I.encrypt(bytes("one"));
    Bytes f2 = I.encrypt(bytes("two"));
    CHECK_THROWS(R.decrypt(f2));  // sequential nonces: skipping f1 fails
  }

  std::printf("== handshake messages are integrity-protected ==\n");
  {
    auto [I, msg0] = NoiseSession::begin_initiator(alice);
    auto [R, msg1] = NoiseSession::accept_responder(bob, msg0);
    Bytes bad = msg1;
    bad[bad.size() - 1] ^= 0x01;
    CHECK_THROWS(I.complete_initiator(bad));
  }
  {
    auto [I, msg0] = NoiseSession::begin_initiator(alice);
    Bytes bad = msg0;
    bad[bad.size() - 1] ^= 0x01;              // hello payload flipped → role check fails
    CHECK_THROWS(NoiseSession::accept_responder(bob, bad));
  }

  std::printf("== prologue must agree ==\n");
  {
    auto [I, msg0] = NoiseSession::begin_initiator(alice, bytes("chain-A"));
    auto [R, msg1] = NoiseSession::accept_responder(bob, msg0, bytes("chain-B"));
    CHECK_THROWS(I.complete_initiator(msg1));
  }
  {
    auto [I, R] = connect(alice, bob, bytes("genesis-hash-here"));
    CHECK(I.open() && R.open());
  }

  std::printf("== a tampered / forged identity proof is rejected ==\n");
  {
    auto [I, msg0] = NoiseSession::begin_initiator(alice);
    auto [R, msg1] = NoiseSession::accept_responder(bob, msg0);
    R.complete_responder(I.complete_initiator(msg1));
    Bytes pi = I.identity_proof();
    pi[pi.size() - 1] ^= 0x01;
    CHECK_THROWS(R.verify_identity_proof(pi));
  }

  std::printf("== MITM with both legs' Noise keys cannot relay an honest proof ==\n");
  {
    // Mallory sits between alice (I) and bob (R) with her own X25519 statics on
    // both legs — the classical break of Noise XX identity (or a quantum
    // recovery of a static). She completes both Noise handshakes and holds both
    // sessions' transport keys. She then tries to forward bob's real identity
    // proof to alice, re-encrypted under the alice↔mallory keys.
    auto mallory_static = crypto::x25519_keygen();
    // Leg 1: alice ↔ mallory-as-responder (raw Noise).
    auto [I, msg0] = NoiseSession::begin_initiator(alice);
    crypto::Noise mr(false, v(mallory_static.public_key), v(mallory_static.secret_key));
    mr.read_message(v(msg0));
    Bytes msg1 = mr.write_message(v(Bytes{0x01, 0x52}));
    Bytes msg2 = I.complete_initiator(msg1);
    mr.read_message(v(msg2));
    mr.split();
    // Leg 2: mallory-as-initiator ↔ bob (raw Noise).
    crypto::Noise mi(true, v(mallory_static.public_key), v(mallory_static.secret_key));
    Bytes m0 = mi.write_message(v(Bytes{0x01, 0x49}));
    auto [R, m1] = NoiseSession::accept_responder(bob, m0);
    mi.read_message(v(m1));
    Bytes m2 = mi.write_message(v(Bytes{0x01, 0x49}));
    R.complete_responder(m2);
    mi.split();
    CHECK(I.handshake_hash() != R.handshake_hash());  // two sessions, two hashes
    // Bob's genuine proof, decrypted by mallory on leg 2, re-sealed on leg 1.
    Bytes bobs_proof_plain = mi.decrypt(v(R.identity_proof()));
    Bytes relayed = mr.encrypt(v(bobs_proof_plain));
    CHECK_THROWS(I.verify_identity_proof(relayed));   // bound to bob's session hash, not alice's
    CHECK(!I.open());                                 // alice never opens the channel
    // Mallory's own hybrid identity would verify — but it is HER key, so
    // alice's pin (peer_identity == bob.sig.public_key) rejects it.
  }

  std::printf("== independent sessions derive independent keys ==\n");
  {
    auto [Ia, Ra] = connect(alice, bob);
    auto [Ib, Rb] = connect(alice, bob);
    CHECK_THROWS(Rb.decrypt(Ia.encrypt(bytes("for Ra only"))));
  }

  return bctest::summary();
}
