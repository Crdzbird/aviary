#pragma once

#include <memory>
#include <utility>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// A confidential, integrity-protected, post-quantum-hybrid, forward-secret
/// peer session (ARCHITECTURE §9). Built from cryptolib's hybrid KEM
/// (X25519 + ML-KEM-768) for key agreement and SecretStream for the framed
/// encrypted transport.
///
/// Handshake (2 messages):
///   initiator → responder:  KEM ciphertext ‖ initiator send-stream header
///   responder → initiator:  responder send-stream header
/// Both sides derive two domain-separated directional keys from the KEM shared
/// secret, so each direction is an independent forward-secure stream.
///
/// NOTE (see docs/IMPROVEMENTS.md): this provides channel confidentiality and a
/// PQ-hybrid key exchange, but not yet mutual *identity* authentication (binding
/// the channel to a known peer/validator key). Full Noise XX mutual auth is
/// C++-only in cryptolib and not bridged to the C ABI; layering a signed
/// handshake transcript is the follow-up.
namespace blockchainer::p2p {

class SecureSession {
 public:
  /// A peer's long-term identity is a hybrid-KEM keypair (responder side).
  static crypto::KeyPair generate_identity() { return crypto::hybrid_kem_keygen(); }

  /// Initiator step 1: produce the first handshake message and a half-open
  /// session (can encrypt; can decrypt only after complete_initiator).
  static std::pair<SecureSession, Bytes> begin_initiator(const Bytes& responder_pk);

  /// Responder: consume msg1 with the identity secret key; returns msg2 and a
  /// fully-open session.
  static std::pair<SecureSession, Bytes> accept_responder(const Bytes& responder_sk,
                                                          const Bytes& msg1);

  /// Initiator step 2: consume msg2 to open the receive direction.
  void complete_initiator(const Bytes& msg2);

  bool open() const { return enc_ != nullptr && dec_ != nullptr; }

  /// Encrypt one frame to send to the peer.
  Bytes encrypt(const Bytes& plaintext);
  /// Decrypt one received frame. Throws on tamper / wrong order / truncation.
  Bytes decrypt(const Bytes& frame);

 private:
  SecureSession() = default;

  std::unique_ptr<crypto::StreamEncryptor> enc_;  // our send direction
  std::unique_ptr<crypto::StreamDecryptor> dec_;  // peer → us direction
  Bytes pending_recv_key_;  // initiator: receive key, until msg2 arrives
};

}  // namespace blockchainer::p2p
