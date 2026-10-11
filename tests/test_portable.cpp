// PortableAccount: move a signing account off the node under a chosen trust
// tier - a shared passphrase, recipient-bound to a device's key (no shared
// secret), or recipient-bound AND authenticated (Fortress-grade). Every tier
// fails closed on tamper / wrong key / wrong passphrase.
#include <string>

#include "aviary/crypto.hpp"
#include "aviary/keyring.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using custody::KeyStore;
using custody::PortableAccount;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
template <class F>
bool fails(F&& f) {
  try {
    f();
    return false;
  } catch (const std::exception&) {
    return true;
  }
}
}  // namespace

int main() {
  init();

  auto kp = crypto::ed25519_keygen();
  const PortableAccount acct{kp.public_key, kp.secret_key};

  std::printf("== payload codec round-trips ==\n");
  {
    PortableAccount back = PortableAccount::decode(view(acct.encode()));
    CHECK(back.public_key == acct.public_key && back.secret == acct.secret);
  }

  std::printf("== tier 1 - passphrase: anyone with the phrase; wrong phrase fails ==\n");
  {
    const Bytes env = acct.seal_passphrase("backup-phrase", /*ops=*/3, /*mem=*/1u << 16);
    PortableAccount back = PortableAccount::open_passphrase(view(env), "backup-phrase");
    CHECK(back.secret == acct.secret);
    CHECK_MSG(fails([&] { PortableAccount::open_passphrase(view(env), "wrong"); }),
              "wrong passphrase fails closed");
  }

  std::printf("== tier 2 - recipient-bound: only the device's KEM secret opens ==\n");
  {
    auto device = crypto::hybrid_kem_keygen();
    auto stranger = crypto::hybrid_kem_keygen();

    const Bytes env = acct.seal_to(view(device.public_key));  // no shared secret
    PortableAccount back = PortableAccount::open_to(view(env), view(device.secret_key));
    CHECK(back.secret == acct.secret);

    CHECK_MSG(fails([&] { PortableAccount::open_to(view(env), view(stranger.secret_key)); }),
              "a different device's key cannot open it");

    Bytes tampered = env;
    tampered[tampered.size() / 2] ^= 0x01;
    CHECK_MSG(fails([&] { PortableAccount::open_to(view(tampered), view(device.secret_key)); }),
              "tampered envelope fails closed");
  }

  std::printf("== tier 3 - signed-recipient: recipient verifies the owner too ==\n");
  {
    auto device = crypto::hybrid_kem_keygen();
    auto owner = crypto::hybrid_sig_keygen();
    auto impostor = crypto::hybrid_sig_keygen();

    const Bytes env = acct.seal_signed_to(view(device.public_key), view(owner.secret_key));
    PortableAccount back =
        PortableAccount::open_signed_from(view(env), view(device.secret_key), view(owner.public_key));
    CHECK(back.secret == acct.secret);

    // Right device key, WRONG expected signer -> rejected (authenticity).
    CHECK_MSG(fails([&] {
                PortableAccount::open_signed_from(view(env), view(device.secret_key),
                                                  view(impostor.public_key));
              }),
              "an envelope not signed by the expected owner is rejected");

    // Right signer, WRONG device key -> rejected (confidentiality).
    auto stranger = crypto::hybrid_kem_keygen();
    CHECK_MSG(fails([&] {
                PortableAccount::open_signed_from(view(env), view(stranger.secret_key),
                                                  view(owner.public_key));
              }),
              "a different device cannot open it");
  }

  std::printf("== KeyStore composition: extract -> seal -> open -> absorb into another store ==\n");
  {
    KeyStore src = KeyStore::create_with_passphrase("store-A");
    const crypto::Hash addr = src.add_account(view(kp.public_key), view(kp.secret_key));

    // The destination device holds a KEM keypair; the source owner a sig key.
    auto device = crypto::hybrid_kem_keygen();
    auto owner = crypto::hybrid_sig_keygen();

    const PortableAccount extracted = src.extract(addr);
    const Bytes env = extracted.seal_signed_to(view(device.public_key), view(owner.secret_key));

    // On the other device: open + verify, then absorb into a fresh store.
    PortableAccount received =
        PortableAccount::open_signed_from(view(env), view(device.secret_key), view(owner.public_key));
    KeyStore dst = KeyStore::create_with_passphrase("store-B");  // independent master
    const crypto::Hash imported = dst.absorb(received);

    CHECK_MSG(imported == addr, "the account keeps its address across the move");
    CHECK(dst.public_key(addr) == kp.public_key);
    CHECK_MSG(dst.reveal_secret(addr) == kp.secret_key, "the secret survives, resealed under B");

    // A locked source can't extract.
    src.lock();
    CHECK_MSG(fails([&] { src.extract(addr); }), "a locked store cannot extract");
  }

  return bctest::summary();
}
