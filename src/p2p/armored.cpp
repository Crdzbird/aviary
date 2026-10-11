#include "aviary/armored.hpp"

#include <stdexcept>

#include "aviary/codec.hpp"

namespace aviary::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// The transcript the initiator signs and the responder re-derives - binds the
// ratchet handshake, the responder's prekey, and both identity keys, so the
// signature cannot be replayed onto a different channel and proves the initiator
// meant to talk to THIS responder. Hashed (domain-tagged) then signed.
Hash transcript_hash(const Bytes& handshake, crypto::ByteView responder_prekey_pub,
                     crypto::ByteView responder_sig_pub, crypto::ByteView initiator_sig_pub) {
  codec::CodecWriter w;
  w.write_bytes(view(handshake));
  w.write_bytes(responder_prekey_pub);
  w.write_bytes(responder_sig_pub);
  w.write_bytes(initiator_sig_pub);
  // kHeader is unused here for its ledger meaning; a dedicated channel tag keeps
  // this signature disjoint from every on-chain signature domain.
  return crypto::hash_tagged(crypto::Domain::kPeerRecord, w.bytes());
}

// The wire handshake: the ratchet handshake, the initiator's identity key, and
// its transcript signature.
Bytes encode_hello(const Bytes& handshake, crypto::ByteView initiator_sig_pub,
                   const Bytes& signature) {
  codec::CodecWriter w;
  w.write_bytes(view(handshake));
  w.write_bytes(initiator_sig_pub);
  w.write_bytes(view(signature));
  return w.take();
}
}  // namespace

std::pair<ArmoredChannel, Bytes> ArmoredChannel::initiate(crypto::ByteView responder_prekey_pub,
                                                         crypto::ByteView responder_sig_pub,
                                                         const crypto::KeyPair& initiator_sig) {
  auto [ratchet, handshake] = crypto::Ratchet::initiate(responder_prekey_pub);
  const Hash th = transcript_hash(handshake, responder_prekey_pub, responder_sig_pub,
                                  view(initiator_sig.public_key));
  const Bytes sig = crypto::hybrid_sig_sign(codec::ByteView(th.data(), th.size()),
                                            view(initiator_sig.secret_key));
  Bytes hello = encode_hello(handshake, view(initiator_sig.public_key), sig);
  // The responder identity we targeted is authenticated implicitly (only its
  // prekey holder can open the channel); confirmed on first successful decrypt.
  Bytes peer_id(responder_sig_pub.begin(), responder_sig_pub.end());
  return {ArmoredChannel(std::move(ratchet), std::move(peer_id)), std::move(hello)};
}

ArmoredChannel ArmoredChannel::accept(const Bytes& handshake, const crypto::KeyPair& prekey,
                                      const crypto::KeyPair& responder_sig) {
  codec::CodecReader r(view(handshake));
  const Bytes ratchet_handshake = r.read_bytes();
  const Bytes initiator_sig_pub = r.read_bytes();
  const Bytes signature = r.read_bytes();
  r.assert_fully_consumed();

  const Hash th = transcript_hash(ratchet_handshake, view(prekey.public_key),
                                  view(responder_sig.public_key), view(initiator_sig_pub));
  if (!crypto::hybrid_sig_verify(codec::ByteView(th.data(), th.size()), view(signature),
                                 view(initiator_sig_pub))) {
    throw std::runtime_error("armored: initiator authentication failed");
  }

  crypto::Ratchet ratchet =
      crypto::Ratchet::accept(view(ratchet_handshake), view(prekey.public_key),
                              view(prekey.secret_key));
  return ArmoredChannel(std::move(ratchet), initiator_sig_pub);
}

Bytes ArmoredChannel::encrypt(const Bytes& plaintext) { return ratchet_.encrypt(view(plaintext)); }
Bytes ArmoredChannel::decrypt(const Bytes& frame) { return ratchet_.decrypt(view(frame)); }

}  // namespace aviary::p2p
