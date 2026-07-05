// S3 custodial key management: Vault (seal/open), Keyring (slots protecting a
// master key), and the KeyStore that seals account signing secrets at rest.
#include <algorithm>
#include <string>
#include <vector>

#include "blockchainer/keyring.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using custody::KeyStore;
using custody::Keyring;
using custody::Vault;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes bytes(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}
}  // namespace

int main() {
  init();

  std::printf("== keyring: a master key unlocks the same value through any slot ==\n");
  {
    Keyring kr = Keyring::create();
    CHECK(kr.add_passphrase_slot("correct horse"));
    Bytes device(32, 0x5A);
    CHECK(kr.add_device_slot(view(device)));
    CHECK(kr.slot_count() == 2);

    Bytes m_pass = kr.unlock_with_passphrase("correct horse");
    Bytes m_dev = kr.unlock_with_device(view(device));
    CHECK(m_pass.size() == 32);
    CHECK(m_pass == m_dev);  // both slots wrap the same master key

    // A wrong passphrase / device factor fails to unlock.
    CHECK(::bctest::throws([&] { (void)kr.unlock_with_passphrase("wrong"); }));
    Bytes wrong_dev(32, 0x00);
    CHECK(::bctest::throws([&] { (void)kr.unlock_with_device(view(wrong_dev)); }));

    std::printf("== keyring serialise/deserialise round-trip ==\n");
    Bytes blob = kr.serialise();
    Keyring loaded = Keyring::deserialise(view(blob));
    CHECK(loaded.slot_count() == 2);
    CHECK(loaded.unlock_with_passphrase("correct horse") == m_pass);
  }

  std::printf("== vault: seal → open round-trips; tamper + wrong-aad rejected ==\n");
  {
    Bytes master(32, 0x11);
    Vault v(view(master));
    Bytes secret = bytes("super secret signing key material");
    auto sealed = v.seal(view(secret), "acct-a");
    CHECK(sealed != secret);
    CHECK(v.open(sealed, "acct-a") == secret);

    // Wrong aad fails.
    CHECK(::bctest::throws([&] { (void)v.open(sealed, "acct-b"); }));
    // Tampered ciphertext fails.
    auto tampered = sealed;
    tampered[tampered.size() / 2] ^= 0x01;
    CHECK(::bctest::throws([&] { (void)v.open(tampered, "acct-a"); }));
    // A vault from a different master cannot open it.
    Bytes other(32, 0x22);
    Vault v2(view(other));
    CHECK(::bctest::throws([&] { (void)v2.open(sealed, "acct-a"); }));
  }

  std::printf("== key store: accounts sealed at rest, revealed only when unlocked ==\n");
  {
    auto alice = crypto::ed25519_keygen_from_seed(view(Bytes(32, 1)));
    auto bob = crypto::ed25519_keygen_from_seed(view(Bytes(32, 2)));

    KeyStore ks = KeyStore::create_with_passphrase("hunter2");
    CHECK(ks.unlocked());
    Hash a_addr = ks.add_account(view(alice.public_key), view(alice.secret_key));
    Hash b_addr = ks.add_account(view(bob.public_key), view(bob.secret_key));
    CHECK(a_addr == crypto::address_of(view(alice.public_key)));
    CHECK(ks.has(a_addr) && ks.has(b_addr));
    CHECK(ks.addresses().size() == 2);
    CHECK(ks.public_key(a_addr) == alice.public_key);
    CHECK(ks.reveal_secret(a_addr) == alice.secret_key);

    std::printf("== key store: persists with no plaintext, reloads locked ==\n");
    Bytes blob = ks.serialise();
    // The plaintext secret must not appear in the serialised blob.
    bool leaked = false;
    for (std::size_t i = 0; i + alice.secret_key.size() <= blob.size(); ++i) {
      if (std::equal(alice.secret_key.begin(), alice.secret_key.end(), blob.begin() + i)) {
        leaked = true;
        break;
      }
    }
    CHECK_MSG(!leaked, "sealed blob contains no plaintext secret");

    KeyStore reloaded = KeyStore::load(view(blob));
    CHECK(!reloaded.unlocked());  // comes back locked
    CHECK(reloaded.has(a_addr));
    CHECK(reloaded.public_key(a_addr) == alice.public_key);  // public data available while locked
    CHECK(::bctest::throws([&] { (void)reloaded.reveal_secret(a_addr); }));  // locked → cannot reveal

    reloaded.unlock_with_passphrase("hunter2");
    CHECK(reloaded.unlocked());
    CHECK(reloaded.reveal_secret(a_addr) == alice.secret_key);
    CHECK(reloaded.reveal_secret(b_addr) == bob.secret_key);

    // Wrong passphrase does not unlock.
    KeyStore r2 = KeyStore::load(view(blob));
    CHECK(::bctest::throws([&] { r2.unlock_with_passphrase("nope"); }));

    std::printf("== a revealed secret actually signs a valid transaction ==\n");
    // Custody → signing: reveal the sealed key and use it exactly like a raw one.
    Bytes secret = reloaded.reveal_secret(a_addr);
    Bytes msg = bytes("sign me");
    Bytes sig = crypto::ed25519_sign(view(msg), view(secret));
    CHECK(crypto::ed25519_verify(view(msg), view(sig), view(alice.public_key)));
  }

  return bctest::summary();
}
