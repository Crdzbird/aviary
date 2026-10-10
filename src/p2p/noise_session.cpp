#include "aviary/noise_session.hpp"

#include <stdexcept>

namespace aviary::p2p {

namespace {
constexpr std::uint8_t kVersion = 0x01;
constexpr std::uint8_t kRoleInitiator = 0x49;  // 'I'
constexpr std::uint8_t kRoleResponder = 0x52;  // 'R'

crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }

// The handshake payloads: a fixed hello the reader checks (fail-closed on a
// version/role mismatch). Also keeps the encrypted payloads non-empty.
Bytes hello(std::uint8_t role) { return Bytes{kVersion, role}; }
void expect_hello(const Bytes& payload, std::uint8_t role, const char* what) {
  if (payload != hello(role)) throw std::runtime_error(std::string("noise: bad hello in ") + what);
}
}  // namespace

NoiseSession::Identity NoiseSession::generate_identity() {
  return Identity{crypto::x25519_keygen(), crypto::hybrid_sig_keygen()};
}

std::pair<NoiseSession, Bytes> NoiseSession::begin_initiator(const Identity& me, const Bytes& prologue) {
  crypto::Noise n(/*initiator=*/true, view(me.noise_static.public_key), view(me.noise_static.secret_key),
                  view(prologue));
  Bytes msg0 = n.write_message(view(hello(kRoleInitiator)));
  return {NoiseSession(std::move(n), me.sig, /*initiator=*/true), std::move(msg0)};
}

std::pair<NoiseSession, Bytes> NoiseSession::accept_responder(const Identity& me, const Bytes& msg0,
                                                              const Bytes& prologue) {
  crypto::Noise n(/*initiator=*/false, view(me.noise_static.public_key), view(me.noise_static.secret_key),
                  view(prologue));
  expect_hello(n.read_message(view(msg0)), kRoleInitiator, "msg0");
  Bytes msg1 = n.write_message(view(hello(kRoleResponder)));
  return {NoiseSession(std::move(n), me.sig, /*initiator=*/false), std::move(msg1)};
}

Bytes NoiseSession::complete_initiator(const Bytes& msg1) {
  if (!initiator_ || split_) throw std::runtime_error("noise: complete_initiator out of order");
  expect_hello(noise_.read_message(view(msg1)), kRoleResponder, "msg1");
  Bytes msg2 = noise_.write_message(view(hello(kRoleInitiator)));
  if (!noise_.handshake_finished()) throw std::runtime_error("noise: handshake did not finish");
  noise_.split();
  split_ = true;
  return msg2;
}

void NoiseSession::complete_responder(const Bytes& msg2) {
  if (initiator_ || split_) throw std::runtime_error("noise: complete_responder out of order");
  expect_hello(noise_.read_message(view(msg2)), kRoleInitiator, "msg2");
  if (!noise_.handshake_finished()) throw std::runtime_error("noise: handshake did not finish");
  noise_.split();
  split_ = true;
}

// kNoiseBinding ‖ signer role ‖ final handshake hash. The role stops a proof
// being reflected back to its author; the hash pins it to this session alone.
Bytes NoiseSession::signable(bool signer_is_initiator) const {
  Hash h = noise_.handshake_hash();
  Bytes m;
  m.reserve(1 + h.size());
  m.push_back(signer_is_initiator ? kRoleInitiator : kRoleResponder);
  m.insert(m.end(), h.begin(), h.end());
  Hash d = crypto::hash_tagged(crypto::Domain::kNoiseBinding, m);
  return Bytes(d.begin(), d.end());
}

Bytes NoiseSession::identity_proof() {
  if (!split_) throw std::runtime_error("noise: identity_proof before the handshake finished");
  if (proof_sent_) throw std::runtime_error("noise: identity_proof already produced");
  proof_sent_ = true;
  Bytes sig = crypto::hybrid_sig_sign(view(signable(initiator_)), view(sig_.secret_key));
  const Bytes& pk = sig_.public_key;
  if (pk.size() > 0xFFFF) throw std::runtime_error("noise: identity key too large");
  Bytes rec;
  rec.reserve(2 + pk.size() + sig.size());
  rec.push_back(static_cast<std::uint8_t>(pk.size() >> 8));
  rec.push_back(static_cast<std::uint8_t>(pk.size()));
  rec.insert(rec.end(), pk.begin(), pk.end());
  rec.insert(rec.end(), sig.begin(), sig.end());
  return noise_.encrypt(view(rec));
}

void NoiseSession::verify_identity_proof(const Bytes& record) {
  if (!split_) throw std::runtime_error("noise: proof before the handshake finished");
  if (authenticated()) throw std::runtime_error("noise: peer already authenticated");
  Bytes rec = noise_.decrypt(view(record));
  if (rec.size() < 2) throw std::runtime_error("noise: truncated identity proof");
  const std::size_t n = (static_cast<std::size_t>(rec[0]) << 8) | rec[1];
  if (rec.size() < 2 + n || n == 0) throw std::runtime_error("noise: malformed identity proof");
  Bytes pk(rec.begin() + 2, rec.begin() + 2 + static_cast<std::ptrdiff_t>(n));
  Bytes sig(rec.begin() + 2 + static_cast<std::ptrdiff_t>(n), rec.end());
  // The peer signed as the OTHER role.
  if (!crypto::hybrid_sig_verify(view(signable(!initiator_)), view(sig), view(pk)))
    throw std::runtime_error("noise: identity proof does not verify for this session");
  peer_identity_ = std::move(pk);
}

Bytes NoiseSession::encrypt(const Bytes& plaintext) {
  if (!open()) throw std::runtime_error("noise: encrypt before the peer is authenticated");
  return noise_.encrypt(view(plaintext));
}
Bytes NoiseSession::decrypt(const Bytes& record) {
  if (!open()) throw std::runtime_error("noise: decrypt before the peer is authenticated");
  return noise_.decrypt(view(record));
}

}  // namespace aviary::p2p
