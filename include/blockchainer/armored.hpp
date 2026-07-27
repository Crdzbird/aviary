#pragma once

#include <utility>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// A mutually-authenticated, post-quantum, forward-secret **and** post-compromise-
/// secure channel (ARCHITECTURE §9, hardened transport tier). It composes two
/// cryptolib pieces the raw ratchet cannot provide alone:
///
///   - confidentiality + forward secrecy + post-compromise healing from the
///     hybrid-KEM Double Ratchet ([crypto::Ratchet]); and
///   - MUTUAL identity authentication from the hybrid signature (Ed25519 +
///     ML-DSA-65) — the same long-term key a peer publishes in its signed PEX
///     record and the secure session authenticates with.
///
/// The raw ratchet authenticates only the responder (via prekey ownership). The
/// [ArmoredChannel] handshake adds the missing half: the initiator signs a
/// transcript binding the ratchet handshake, the responder's prekey, and both
/// identity keys, so the responder learns *who* dialed it and can pin that
/// identity against its allow-list. Result — neither side can be impersonated,
/// nothing is readable by a quantum "harvest now" adversary, and a key leak on
/// a long-lived validator link heals as messages flow.
///
/// The responder's prekey must reach the initiator authentically; the canonical
/// source is the peer's self-signed [p2p::PeerRecord] (its `prekey` field is
/// covered by the record signature), so `initiate` takes the prekey and the
/// identity key exactly as they appear there.
namespace blockchainer::p2p {

class ArmoredChannel {
 public:
  /// Initiator: open a channel to a responder whose authenticated prekey public
  /// key is [responder_prekey_pub] and whose long-term identity is
  /// [responder_sig_pub] (both from the peer's verified record). Signs the
  /// transcript with [initiator_sig]. Returns the open channel (can encrypt
  /// immediately) and the handshake message to send. Throws on failure.
  static std::pair<ArmoredChannel, Bytes> initiate(crypto::ByteView responder_prekey_pub,
                                                   crypto::ByteView responder_sig_pub,
                                                   const crypto::KeyPair& initiator_sig);

  /// Responder: consume the initiator's [handshake] with our [prekey] (public +
  /// secret) and our [responder_sig] identity. Verifies the initiator's
  /// transcript signature (throws on failure); on success `peer_identity()` is
  /// the authenticated initiator signing key. Returns the open channel.
  static ArmoredChannel accept(const Bytes& handshake, const crypto::KeyPair& prekey,
                               const crypto::KeyPair& responder_sig);

  bool open() const { return ratchet_.open(); }

  /// The authenticated remote signing public key. For the responder this is the
  /// verified initiator key; for the initiator, the responder key it targeted
  /// (confirmed once a message from the responder decrypts). Check it against
  /// your peer allow-list.
  const Bytes& peer_identity() const { return peer_identity_; }

  /// Encrypt one frame to the peer (advances the ratchet).
  Bytes encrypt(const Bytes& plaintext);
  /// Decrypt one received frame. Throws on tamper / wrong key / replay.
  Bytes decrypt(const Bytes& frame);

 private:
  ArmoredChannel(crypto::Ratchet ratchet, Bytes peer_identity)
      : ratchet_(std::move(ratchet)), peer_identity_(std::move(peer_identity)) {}

  crypto::Ratchet ratchet_;
  Bytes peer_identity_;
};

}  // namespace blockchainer::p2p
