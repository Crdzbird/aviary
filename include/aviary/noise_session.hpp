#pragma once

#include <utility>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// A **standards-based** peer session: Noise_XX_25519_ChaChaPoly_SHA256 for the
/// channel, plus a post-quantum identity binding on top (ARCHITECTURE §9, tier
/// `kNoiseXX`). Where [SecureSession] is our own SIGMA construction and
/// [ArmoredChannel] a Double Ratchet, this tier speaks the Noise XX that every
/// other Noise implementation speaks — the choice for interoperating with peers
/// that are not aviary nodes.
///
/// Handshake (3 Noise messages, then one identity proof each way):
///   initiator → responder:  msg0  = e                     ‖ hello
///   responder → initiator:  msg1  = e, ee, s, es          ‖ {hello}
///   initiator → responder:  msg2  = s, se                 ‖ {hello}
///   both, after Split:      proof = {sig_pk ‖ HybridSig(kNoiseBinding ‖ role ‖ handshake_hash)}
///
/// Why the extra proof: Noise XX authenticates each side by its X25519 static —
/// classical only. A "harvest now, break later" quantum adversary who recovers
/// an X25519 static could impersonate that peer in a future Noise handshake. The
/// proof binds the peer's long-term **hybrid (Ed25519 + ML-DSA-65)** identity to
/// THIS session's final handshake hash, which commits every handshake byte incl.
/// both statics and ephemerals. A MITM runs two sessions with two different
/// hashes, so an honest party's proof never verifies in the other one; and the
/// ML-DSA half cannot be forged. Authentication therefore survives a quantum
/// adversary; **confidentiality of this tier does not** (X25519 only) — pick
/// `kMutual`/`kRatcheted` when PQ confidentiality is required.
///
/// Fail-closed: `encrypt`/`decrypt` refuse until the peer's proof verified. The
/// caller MUST still pin `peer_identity()` against its allow-list — the proof
/// shows the peer holds that key, not that it is the peer you wanted.
namespace aviary::p2p {

class NoiseSession {
 public:
  struct Identity {
    crypto::KeyPair noise_static;  ///< X25519 static (may be per-boot: identity lives in .sig)
    crypto::KeyPair sig;           ///< hybrid signature (Ed25519 + ML-DSA-65) — the identity
  };
  static Identity generate_identity();

  /// Initiator step 1 → msg0.
  static std::pair<NoiseSession, Bytes> begin_initiator(const Identity& me, const Bytes& prologue = {});
  /// Responder: consume msg0 → msg1. Throws on a malformed msg0.
  static std::pair<NoiseSession, Bytes> accept_responder(const Identity& me, const Bytes& msg0,
                                                         const Bytes& prologue = {});
  /// Initiator step 2: consume msg1 → msg2; transport keys are derived. Throws on failure.
  Bytes complete_initiator(const Bytes& msg1);
  /// Responder step 2: consume msg2; transport keys are derived. Throws on failure.
  void complete_responder(const Bytes& msg2);

  /// True once the three Noise messages are processed and keys are split.
  bool handshake_done() const { return split_; }
  /// This side's identity proof (an encrypted transport record). Send it right
  /// after the handshake. Throws before the handshake is done or if called twice.
  Bytes identity_proof();
  /// Verify the peer's identity proof: decrypts it, checks the hybrid signature
  /// over THIS session's handshake hash and the peer's role. Throws on any
  /// failure; on success `peer_identity()` is set.
  void verify_identity_proof(const Bytes& record);

  bool authenticated() const { return !peer_identity_.empty(); }
  bool open() const { return split_ && authenticated(); }
  /// The authenticated remote hybrid signing key. Pin it against your allow-list.
  const Bytes& peer_identity() const { return peer_identity_; }
  /// The peer's X25519 static learned in-band (Noise-level identity).
  Bytes peer_static() const { return noise_.remote_static(); }
  Hash handshake_hash() const { return noise_.handshake_hash(); }

  /// Transport records — refuse (throw) until `open()`.
  Bytes encrypt(const Bytes& plaintext);
  Bytes decrypt(const Bytes& record);

 private:
  NoiseSession(crypto::Noise noise, crypto::KeyPair sig, bool initiator)
      : noise_(std::move(noise)), sig_(std::move(sig)), initiator_(initiator) {}

  Bytes signable(bool signer_is_initiator) const;

  crypto::Noise noise_;
  crypto::KeyPair sig_;
  bool initiator_;
  bool split_ = false;
  bool proof_sent_ = false;
  Bytes peer_identity_;
};

}  // namespace aviary::p2p
