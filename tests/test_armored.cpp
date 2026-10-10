// ArmoredChannel: a mutually-authenticated, post-quantum, forward-secret channel
// = hybrid-KEM Double Ratchet (confidentiality + FS + post-compromise healing)
// composed with a hybrid-signature identity handshake (mutual authentication).
// The responder's authenticated prekey is sourced from its signed PEX record.
#include <string>
#include <vector>

#include "aviary/armored.hpp"
#include "aviary/crypto.hpp"
#include "aviary/peer_book.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using p2p::ArmoredChannel;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Bytes msg(const char* s) { return Bytes(s, s + std::char_traits<char>::length(s)); }
}  // namespace

int main() {
  init();

  std::printf("== crypto::Ratchet: bidirectional encrypted messaging over many turns ==\n");
  {
    crypto::KeyPair prekey = crypto::Ratchet::generate_prekey();
    auto [initiator, handshake] = crypto::Ratchet::initiate(view(prekey.public_key));
    crypto::Ratchet responder =
        crypto::Ratchet::accept(view(handshake), view(prekey.public_key), view(prekey.secret_key));

    // 20 messages each direction — the ratchet turns repeatedly.
    for (int i = 0; i < 20; ++i) {
      Bytes a = msg("initiator→responder");
      a.push_back(static_cast<std::uint8_t>(i));
      Bytes ct = initiator.encrypt(view(a));
      CHECK(responder.decrypt(view(ct)) == a);

      Bytes b = msg("responder→initiator");
      b.push_back(static_cast<std::uint8_t>(i));
      Bytes ct2 = responder.encrypt(view(b));
      CHECK(initiator.decrypt(view(ct2)) == b);
    }
  }

  std::printf("== a replayed frame is rejected (message keys don't survive use) ==\n");
  {
    crypto::KeyPair prekey = crypto::Ratchet::generate_prekey();
    auto [initiator, handshake] = crypto::Ratchet::initiate(view(prekey.public_key));
    crypto::Ratchet responder =
        crypto::Ratchet::accept(view(handshake), view(prekey.public_key), view(prekey.secret_key));
    Bytes ct = initiator.encrypt(view(msg("once")));
    CHECK(responder.decrypt(view(ct)) == msg("once"));
    bool threw = false;
    try {
      (void)responder.decrypt(view(ct));  // replay
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "the same frame cannot be decrypted twice");
  }

  std::printf("== ArmoredChannel: mutual-auth handshake, then encrypted messaging ==\n");
  {
    crypto::KeyPair r_id = crypto::hybrid_sig_keygen();  // responder identity
    crypto::KeyPair i_id = crypto::hybrid_sig_keygen();  // initiator identity
    crypto::KeyPair r_pre = crypto::Ratchet::generate_prekey();

    auto [initiator, hello] = ArmoredChannel::initiate(view(r_pre.public_key), view(r_id.public_key), i_id);
    ArmoredChannel responder = ArmoredChannel::accept(hello, r_pre, r_id);

    // The responder learned WHO dialed it; the initiator targeted the responder.
    CHECK_MSG(responder.peer_identity() == i_id.public_key, "responder authenticated the initiator");
    CHECK_MSG(initiator.peer_identity() == r_id.public_key, "initiator pinned the responder identity");

    Bytes ct = initiator.encrypt(msg("hello armored"));
    CHECK(responder.decrypt(ct) == msg("hello armored"));
    Bytes ct2 = responder.encrypt(msg("hi back"));
    CHECK(initiator.decrypt(ct2) == msg("hi back"));
  }

  std::printf("== impostor rejected: a handshake signed by the wrong identity fails ==\n");
  {
    crypto::KeyPair r_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair attacker = crypto::hybrid_sig_keygen();  // not who it will claim
    crypto::KeyPair r_pre = crypto::Ratchet::generate_prekey();

    // The attacker signs the transcript with its own key but the wire claims a
    // different identity — the responder recomputes and the signature won't check.
    auto [chan, hello] = ArmoredChannel::initiate(view(r_pre.public_key), view(r_id.public_key), attacker);
    // Corrupt the claimed identity inside the hello (swap in a stranger's pk).
    crypto::KeyPair stranger = crypto::hybrid_sig_keygen();
    codec::CodecReader rd(view(hello));
    const Bytes rh = rd.read_bytes();
    (void)rd.read_bytes();  // drop the real claimed pk
    const Bytes sig = rd.read_bytes();
    codec::CodecWriter w;
    w.write_bytes(view(rh));
    w.write_bytes(view(stranger.public_key));  // lie about identity
    w.write_bytes(view(sig));
    const Bytes forged = w.take();

    bool threw = false;
    try {
      (void)ArmoredChannel::accept(forged, r_pre, r_id);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a forged identity claim fails authentication");
  }

  std::printf("== responder auth by prekey: only the prekey holder can open the channel ==\n");
  {
    crypto::KeyPair r_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair i_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair real_pre = crypto::Ratchet::generate_prekey();
    crypto::KeyPair impostor_pre = crypto::Ratchet::generate_prekey();  // attacker's own prekey

    auto [initiator, hello] = ArmoredChannel::initiate(view(real_pre.public_key), view(r_id.public_key), i_id);
    // An impostor without the real prekey secret tries to accept — encapsulation
    // was to real_pre.public_key, so accepting with a different prekey fails.
    bool threw = false;
    try {
      (void)ArmoredChannel::accept(hello, impostor_pre, r_id);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "an impostor lacking the prekey secret cannot open the channel");
  }

  std::printf("== a tampered ciphertext frame fails closed ==\n");
  {
    crypto::KeyPair r_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair i_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair r_pre = crypto::Ratchet::generate_prekey();
    auto [initiator, hello] = ArmoredChannel::initiate(view(r_pre.public_key), view(r_id.public_key), i_id);
    ArmoredChannel responder = ArmoredChannel::accept(hello, r_pre, r_id);
    Bytes ct = initiator.encrypt(msg("secret payload"));
    ct[ct.size() / 2] ^= 0x01;
    bool threw = false;
    try {
      (void)responder.decrypt(ct);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "tampered ciphertext is rejected");
  }

  std::printf("== composition: the responder prekey comes from its signed PEX record ==\n");
  {
    crypto::KeyPair r_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair i_id = crypto::hybrid_sig_keygen();
    crypto::KeyPair r_kem = crypto::hybrid_kem_keygen();
    crypto::KeyPair r_pre = crypto::Ratchet::generate_prekey();

    // The responder publishes a self-signed record carrying its ratchet prekey.
    p2p::PeerRecord rec{7, "127.0.0.1", 47000, r_kem.public_key, r_id.public_key, r_pre.public_key, {}};
    rec.sign(r_id.secret_key);
    CHECK_MSG(rec.verify(), "the record (incl. prekey) is authentically signed");

    // Tampering with the prekey breaks the record signature — an attacker can't
    // substitute its own prekey to MITM the channel.
    p2p::PeerRecord tampered = rec;
    tampered.prekey = crypto::Ratchet::generate_prekey().public_key;
    CHECK_MSG(!tampered.verify(), "a swapped prekey invalidates the record signature");

    // A dialer opens an armored channel straight from the verified record.
    auto [initiator, hello] = ArmoredChannel::initiate(view(rec.prekey), view(rec.sig_pk), i_id);
    ArmoredChannel responder = ArmoredChannel::accept(hello, r_pre, r_id);
    CHECK(responder.peer_identity() == i_id.public_key);
    Bytes ct = initiator.encrypt(msg("via PEX"));
    CHECK(responder.decrypt(ct) == msg("via PEX"));
  }

  return bctest::summary();
}
