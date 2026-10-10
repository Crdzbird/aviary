#include "aviary/secure_session.hpp"

#include <stdexcept>

#include "aviary/codec.hpp"

namespace aviary::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Domain-separated directional keys derived from the KEM shared secret.
constexpr const char* kCtxI2R = "aviary-p2p-i2r-v1";
constexpr const char* kCtxR2I = "aviary-p2p-r2i-v1";
// Channel binder: proves knowledge of the shared secret inside the signed
// transcript, so a transcript signature is bound to this exact key agreement.
constexpr const char* kCtxBind = "aviary-p2p-bind-v1";

// Transcript role tags — a signature for one role can never verify as the other.
constexpr std::uint8_t kRoleInitiator = 0x01;
constexpr std::uint8_t kRoleResponder = 0x02;

// The transcript "core" both parties can reconstruct after msg1: it commits the
// intended responder, the KEM ciphertext, the initiator's stream header and
// identity, and the channel binder.
Bytes transcript_core(const Bytes& responder_kem_pk, const Bytes& ciphertext,
                      const Bytes& initiator_header, const Bytes& initiator_id_pk,
                      const Bytes& binder) {
  codec::CodecWriter w;
  w.write_bytes(view(responder_kem_pk));
  w.write_bytes(view(ciphertext));
  w.write_bytes(view(initiator_header));
  w.write_bytes(view(initiator_id_pk));
  w.write_bytes(view(binder));
  return w.take();
}

// The bytes a party signs: role ‖ core ‖ suffix (suffix is empty for the
// initiator; the responder appends its header + identity).
Bytes signable(std::uint8_t role, const Bytes& core, const Bytes& suffix) {
  codec::CodecWriter w;
  w.write_u8(role);
  w.write_bytes(view(core));
  w.write_bytes(view(suffix));
  return w.take();
}

Bytes responder_suffix(const Bytes& responder_header, const Bytes& responder_id_pk) {
  codec::CodecWriter w;
  w.write_bytes(view(responder_header));
  w.write_bytes(view(responder_id_pk));
  return w.take();
}
}  // namespace

SecureSession::Identity SecureSession::generate_identity() {
  return Identity{crypto::hybrid_kem_keygen(), crypto::hybrid_sig_keygen()};
}

std::pair<SecureSession, Bytes> SecureSession::begin_initiator(
    const Bytes& responder_kem_pk, const crypto::KeyPair& initiator_sig) {
  crypto::KemEncaps kem = crypto::hybrid_kem_encapsulate(view(responder_kem_pk));
  const Bytes k_i2r = crypto::derive_key(kCtxI2R, view(kem.shared_secret));
  const Bytes k_r2i = crypto::derive_key(kCtxR2I, view(kem.shared_secret));
  const Bytes binder = crypto::derive_key(kCtxBind, view(kem.shared_secret));

  SecureSession s;
  s.enc_ = std::make_unique<crypto::StreamEncryptor>(view(k_i2r));
  s.pending_recv_key_ = k_r2i;

  const Bytes core = transcript_core(responder_kem_pk, kem.ciphertext, s.enc_->header(),
                                     initiator_sig.public_key, binder);
  const Bytes sig_i =
      crypto::hybrid_sig_sign(view(signable(kRoleInitiator, core, {})), view(initiator_sig.secret_key));
  s.pending_transcript_ = core;  // to verify the responder's signature later

  // msg1 = ciphertext ‖ initiator header ‖ initiator identity pk ‖ sig_I.
  codec::CodecWriter w;
  w.write_bytes(view(kem.ciphertext));
  w.write_bytes(view(s.enc_->header()));
  w.write_bytes(view(initiator_sig.public_key));
  w.write_bytes(view(sig_i));
  return {std::move(s), w.take()};
}

std::pair<SecureSession, Bytes> SecureSession::accept_responder(const Identity& responder,
                                                                const Bytes& msg1) {
  codec::CodecReader r(view(msg1));
  const Bytes ciphertext = r.read_bytes();
  const Bytes initiator_header = r.read_bytes();
  const Bytes initiator_id_pk = r.read_bytes();
  const Bytes sig_i = r.read_bytes();
  r.assert_fully_consumed();

  const Bytes ss = crypto::hybrid_kem_decapsulate(view(ciphertext), view(responder.kem.secret_key));
  const Bytes k_i2r = crypto::derive_key(kCtxI2R, view(ss));
  const Bytes k_r2i = crypto::derive_key(kCtxR2I, view(ss));
  const Bytes binder = crypto::derive_key(kCtxBind, view(ss));

  // Authenticate the initiator. A wrong KEM key yields a different shared secret
  // → a different binder → this signature fails to verify, so an impostor (or a
  // key mismatch) aborts the handshake here rather than opening a bad channel.
  const Bytes core = transcript_core(responder.kem.public_key, ciphertext, initiator_header,
                                     initiator_id_pk, binder);
  if (!crypto::hybrid_sig_verify(view(signable(kRoleInitiator, core, {})), view(sig_i),
                                 view(initiator_id_pk))) {
    throw std::runtime_error("secure_session: initiator authentication failed");
  }

  SecureSession s;
  // Receive from the initiator (i2r); send back to it (r2i).
  s.dec_ = std::make_unique<crypto::StreamDecryptor>(view(k_i2r), view(initiator_header));
  s.enc_ = std::make_unique<crypto::StreamEncryptor>(view(k_r2i));
  s.peer_identity_ = initiator_id_pk;  // authenticated

  const Bytes suffix = responder_suffix(s.enc_->header(), responder.sig.public_key);
  const Bytes sig_r =
      crypto::hybrid_sig_sign(view(signable(kRoleResponder, core, suffix)), view(responder.sig.secret_key));

  // msg2 = responder header ‖ responder identity pk ‖ sig_R.
  codec::CodecWriter w;
  w.write_bytes(view(s.enc_->header()));
  w.write_bytes(view(responder.sig.public_key));
  w.write_bytes(view(sig_r));
  return {std::move(s), w.take()};
}

void SecureSession::complete_initiator(const Bytes& msg2) {
  codec::CodecReader r(view(msg2));
  const Bytes responder_header = r.read_bytes();
  const Bytes responder_id_pk = r.read_bytes();
  const Bytes sig_r = r.read_bytes();
  r.assert_fully_consumed();

  // Authenticate the responder over the full transcript (our core + its suffix).
  const Bytes suffix = responder_suffix(responder_header, responder_id_pk);
  if (!crypto::hybrid_sig_verify(view(signable(kRoleResponder, pending_transcript_, suffix)),
                                 view(sig_r), view(responder_id_pk))) {
    throw std::runtime_error("secure_session: responder authentication failed");
  }

  dec_ = std::make_unique<crypto::StreamDecryptor>(view(pending_recv_key_), view(responder_header));
  peer_identity_ = responder_id_pk;  // authenticated
  pending_recv_key_.clear();
  pending_transcript_.clear();
}

Bytes SecureSession::encrypt(const Bytes& plaintext) {
  return enc_->push(view(plaintext), /*final_chunk=*/false);
}

Bytes SecureSession::decrypt(const Bytes& frame) {
  return dec_->pull(view(frame));
}

}  // namespace aviary::p2p
