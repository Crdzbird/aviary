#pragma once

#include <memory>
#include <utility>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// A confidential, integrity-protected, post-quantum-hybrid, forward-secret,
/// **mutually authenticated** peer session (ARCHITECTURE §9). Built from
/// cryptolib's hybrid KEM (X25519 + ML-KEM-768) for key agreement, SecretStream
/// for the framed encrypted transport, and a hybrid signature (Ed25519 +
/// ML-DSA-65) over the handshake transcript for identity authentication.
///
/// Handshake (2 messages, SIGMA-style signed transcript):
///   initiator → responder:  KEM ciphertext ‖ initiator stream header
///                           ‖ initiator identity pk ‖ sig_I(transcript)
///   responder → initiator:  responder stream header
///                           ‖ responder identity pk ‖ sig_R(transcript)
/// Both sides derive two domain-separated directional keys from the KEM shared
/// secret, so each direction is an independent forward-secure stream. Each side
/// signs a transcript that binds the KEM ciphertext, both stream headers, both
/// identity keys, and a *channel binder* derived from the shared secret — so a
/// signature cannot be replayed onto a different session, and a party must have
/// actually agreed the key to produce a valid one.
///
/// Why this matters post-quantum: confidentiality survives a quantum adversary
/// via ML-KEM, and **authentication survives via ML-DSA** — a "harvest now,
/// impersonate later" attacker who breaks X25519/Ed25519 still cannot forge the
/// PQ half of the transcript signature, so it cannot MITM the channel.
///
/// After the handshake each side exposes `peer_identity()` — the *authenticated*
/// remote signing public key. Callers MUST check it against their expected peer
/// / validator allow-list; the handshake proves the peer holds that key but not
/// that it is the peer you wanted (classic Noise-XX identity pinning).
namespace aviary::p2p {

class SecureSession {
 public:
  /// A peer's long-term identity: a hybrid KEM keypair (to receive handshakes)
  /// and a hybrid signature keypair (to authenticate). An initiator only needs
  /// the signature keypair; a responder needs both.
  struct Identity {
    crypto::KeyPair kem;  ///< hybrid KEM (X25519 + ML-KEM-768)
    crypto::KeyPair sig;  ///< hybrid signature (Ed25519 + ML-DSA-65)
  };

  /// Fresh long-term identity (both keypairs).
  static Identity generate_identity();

  /// Initiator step 1: produce the first handshake message and a half-open
  /// session (can encrypt; can decrypt only after complete_initiator). The
  /// initiator authenticates with [initiator_sig] and encapsulates to the
  /// responder's known KEM public key [responder_kem_pk].
  static std::pair<SecureSession, Bytes> begin_initiator(
      const Bytes& responder_kem_pk, const crypto::KeyPair& initiator_sig);

  /// Responder: consume msg1 with the long-term [responder] identity; verifies
  /// the initiator's transcript signature (throws on failure), then returns msg2
  /// and a fully-open session. On success `peer_identity()` is the authenticated
  /// initiator signing key.
  static std::pair<SecureSession, Bytes> accept_responder(const Identity& responder,
                                                          const Bytes& msg1);

  /// Initiator step 2: consume msg2, verify the responder's transcript signature
  /// (throws on failure), and open the receive direction. On success
  /// `peer_identity()` is the authenticated responder signing key.
  void complete_initiator(const Bytes& msg2);

  bool open() const { return enc_ != nullptr && dec_ != nullptr; }

  /// The authenticated remote signing public key. Empty until this side's
  /// handshake step verifies. Check it against your peer allow-list.
  const Bytes& peer_identity() const { return peer_identity_; }

  /// Encrypt one frame to send to the peer.
  Bytes encrypt(const Bytes& plaintext);
  /// Decrypt one received frame. Throws on tamper / wrong order / truncation.
  Bytes decrypt(const Bytes& frame);

 private:
  SecureSession() = default;

  std::unique_ptr<crypto::StreamEncryptor> enc_;  // our send direction
  std::unique_ptr<crypto::StreamDecryptor> dec_;  // peer → us direction
  Bytes pending_recv_key_;    // initiator: receive key, until msg2 arrives
  Bytes pending_transcript_;  // initiator: signed transcript core, until msg2
  Bytes peer_identity_;       // authenticated remote signing pk
};

}  // namespace aviary::p2p
