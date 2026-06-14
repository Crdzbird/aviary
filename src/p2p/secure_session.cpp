#include "blockchainer/secure_session.hpp"

#include "blockchainer/codec.hpp"

namespace blockchainer::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Domain-separated directional keys derived from the KEM shared secret.
constexpr const char* kCtxI2R = "blockchainer-p2p-i2r-v1";
constexpr const char* kCtxR2I = "blockchainer-p2p-r2i-v1";
}  // namespace

std::pair<SecureSession, Bytes> SecureSession::begin_initiator(
    const Bytes& responder_pk) {
  crypto::KemEncaps kem = crypto::hybrid_kem_encapsulate(view(responder_pk));
  const Bytes k_i2r = crypto::derive_key(kCtxI2R, view(kem.shared_secret));
  const Bytes k_r2i = crypto::derive_key(kCtxR2I, view(kem.shared_secret));

  SecureSession s;
  s.enc_ = std::make_unique<crypto::StreamEncryptor>(view(k_i2r));
  s.pending_recv_key_ = k_r2i;

  // msg1 = KEM ciphertext ‖ initiator send-stream header.
  codec::CodecWriter w;
  w.write_bytes(view(kem.ciphertext));
  w.write_bytes(view(s.enc_->header()));
  return {std::move(s), w.take()};
}

std::pair<SecureSession, Bytes> SecureSession::accept_responder(
    const Bytes& responder_sk, const Bytes& msg1) {
  codec::CodecReader r(view(msg1));
  const Bytes ciphertext = r.read_bytes();
  const Bytes initiator_header = r.read_bytes();
  r.assert_fully_consumed();

  const Bytes ss = crypto::hybrid_kem_decapsulate(view(ciphertext), view(responder_sk));
  const Bytes k_i2r = crypto::derive_key(kCtxI2R, view(ss));
  const Bytes k_r2i = crypto::derive_key(kCtxR2I, view(ss));

  SecureSession s;
  // Receive from the initiator (i2r); send back to it (r2i).
  s.dec_ = std::make_unique<crypto::StreamDecryptor>(view(k_i2r), view(initiator_header));
  s.enc_ = std::make_unique<crypto::StreamEncryptor>(view(k_r2i));

  codec::CodecWriter w;
  w.write_bytes(view(s.enc_->header()));  // msg2 = responder send-stream header
  return {std::move(s), w.take()};
}

void SecureSession::complete_initiator(const Bytes& msg2) {
  codec::CodecReader r(view(msg2));
  const Bytes responder_header = r.read_bytes();
  r.assert_fully_consumed();
  dec_ = std::make_unique<crypto::StreamDecryptor>(view(pending_recv_key_),
                                                   view(responder_header));
  pending_recv_key_.clear();
}

Bytes SecureSession::encrypt(const Bytes& plaintext) {
  return enc_->push(view(plaintext), /*final_chunk=*/false);
}

Bytes SecureSession::decrypt(const Bytes& frame) {
  return dec_->pull(view(frame));
}

}  // namespace blockchainer::p2p
