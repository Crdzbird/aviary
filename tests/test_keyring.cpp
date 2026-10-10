// S3 custodial key management: Vault (seal/open), Keyring (slots protecting a
// master key), and the KeyStore that seals account signing secrets at rest.
#include <algorithm>
#include <string>
#include <vector>

#include "aviary/keyring.hpp"
#include "aviary/custody_signing.hpp"
#include "aviary/node/demo_node.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
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

  std::printf("== vault: molecular (cascade) seal — round-trip, tamper + wrong-aad rejected ==\n");
  {
    Bytes master(32, 0x42);
    Vault v(view(master));
    const Bytes secret = bytes("cascade-sealed signing secret");
    const custody::SealedBlob blob = v.seal_max(view(secret), "acct");
    CHECK(v.open_max(blob, "acct") == secret);

    custody::SealedBlob tampered = blob;
    tampered[tampered.size() / 2] ^= 0x01;
    bool threw = false;
    try {
      (void)v.open_max(tampered, "acct");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a tampered molecular envelope fails closed");

    threw = false;
    try {
      (void)v.open_max(blob, "other-acct");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "the cascade commits to the AAD — relocation rejected");

    Bytes other_master(32, 0x43);
    Vault w(view(other_master));
    threw = false;
    try {
      (void)w.open_max(blob, "acct");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "wrong master key rejected");
  }

  std::printf("== key store v2 round-trip: molecular entries survive persist + reload ==\n");
  {
    auto kp = crypto::ed25519_keygen();
    KeyStore ks = KeyStore::create_with_passphrase("hunter2");
    const crypto::Hash addr = ks.add_account(view(kp.public_key), view(kp.secret_key));
    const Bytes blob = ks.serialise();
    CHECK_MSG(blob[0] == 2, "stores now serialise as format v2");

    KeyStore re = KeyStore::load(view(blob));
    CHECK(!re.unlocked());
    re.unlock_with_passphrase("hunter2");
    CHECK_MSG(re.reveal_secret(addr) == kp.secret_key, "a molecular-sealed entry reopens after reload");
  }

  std::printf("== backward compat: a legacy v1 store (committing-AEAD entries) still opens ==\n");
  {
    // Hand-assemble a v1 blob through public API only: a Keyring whose master we
    // recover, a Vault that seals the old way, and the v1 wire layout.
    auto kp = crypto::ed25519_keygen();
    const crypto::Hash addr = crypto::address_of(view(kp.public_key));
    Keyring kr = Keyring::create();
    CHECK(kr.add_passphrase_slot("legacy-pass"));
    const Bytes master = kr.unlock_with_passphrase("legacy-pass");
    Vault v(view(master));
    // v1 sealed the secret with the committing AEAD, AAD = hex(address).
    static const char* kHex = "0123456789abcdef";
    std::string aad;
    for (std::uint8_t b : addr) {
      aad.push_back(kHex[b >> 4]);
      aad.push_back(kHex[b & 0x0F]);
    }
    const custody::SealedBlob sealed = v.seal(view(kp.secret_key), aad);

    codec::CodecWriter w;
    w.write_u8(1);  // legacy format version
    const Bytes kr_blob = kr.serialise();
    w.write_bytes(codec::ByteView(kr_blob.data(), kr_blob.size()));
    w.write_u32(1);
    w.write_fixed_bytes(codec::ByteView(addr.data(), addr.size()), 32);
    w.write_bytes(codec::ByteView(kp.public_key.data(), kp.public_key.size()));
    w.write_bytes(codec::ByteView(sealed.data(), sealed.size()));
    const Bytes v1_blob = w.take();

    KeyStore ks = KeyStore::load(view(v1_blob));
    ks.unlock_with_passphrase("legacy-pass");
    CHECK_MSG(ks.reveal_secret(addr) == kp.secret_key, "a v1 entry opens via the committing path");
    // And once re-serialised it carries the v2 format with the entry's true seal version.
    const Bytes re = ks.serialise();
    CHECK(re[0] == 2);
    KeyStore ks2 = KeyStore::load(view(re));
    ks2.unlock_with_passphrase("legacy-pass");
    CHECK_MSG(ks2.reveal_secret(addr) == kp.secret_key, "v1→v2 migration preserves the entry");
  }

  std::printf("== portable export/import: an account moves between stores under a passphrase ==\n");
  {
    auto kp = crypto::ed25519_keygen();
    KeyStore src = KeyStore::create_with_passphrase("store-A");
    const crypto::Hash addr = src.add_account(view(kp.public_key), view(kp.secret_key));

    // Light Argon2id work factors for the test; 0,0 (SENSITIVE) is the API default.
    const Bytes envelope = src.export_account(addr, "backup-phrase", /*ops=*/3, /*mem=*/1u << 16);
    CHECK(!envelope.empty());

    KeyStore dst = KeyStore::create_with_passphrase("store-B");  // different keyring/master
    const crypto::Hash imported = dst.import_account(view(envelope), "backup-phrase");
    CHECK_MSG(imported == addr, "the imported account keeps its address");
    CHECK(dst.public_key(addr) == kp.public_key);
    CHECK_MSG(dst.reveal_secret(addr) == kp.secret_key, "the secret survives the round-trip resealed");

    bool threw = false;
    try {
      (void)dst.import_account(view(envelope), "wrong-phrase");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a wrong passphrase fails closed");

    Bytes tampered = envelope;
    tampered[tampered.size() / 2] ^= 0x01;
    threw = false;
    try {
      (void)dst.import_account(view(tampered), "backup-phrase");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a tampered envelope fails closed");
  }

  {
    // Signing from the store: the secret stays inside; ed25519, fee, hybrid, locked.
    node::DemoNode n(0x5151);
    KeyStore ks = KeyStore::create_with_passphrase("hunter2");
    auto alice = n.new_account();
    auto hyb = n.new_account_hybrid();
    auto bob = n.new_account();
    const Hash a = ks.add_account(view(alice.public_key), view(alice.secret_key));
    const Hash h = ks.add_account(view(hyb.public_key), view(hyb.secret_key));
    const Hash b = crypto::address_of(view(bob.public_key));
    n.credit(a, 1000);
    n.credit(h, 500);
    Transaction t1 = custody::sign_transfer(ks, a, b, 100, 0, 0, n.chain_id());
    CHECK(t1.version == 1);
    n.submit(t1);
    Transaction t2 = custody::sign_transfer(ks, h, b, 50, 0, 0, n.chain_id());
    CHECK(t2.version == 2 && t2.sig_algo == static_cast<std::uint8_t>(SigAlgo::kHybridEd25519MlDsa));
    n.submit_encoded(view(t2.encode()));
    CHECK(n.produce_block(1) == 1);
    CHECK(n.balance(b) == 150);
    Transaction t3 = custody::sign_transfer(ks, a, b, 1, 1, 7, n.chain_id());
    CHECK(t3.version == 2 && t3.fee == 7);
    ks.lock();
    bool locked = false;
    try { (void)custody::sign_transfer(ks, a, b, 1, 2, 0, n.chain_id()); } catch (const std::exception&) { locked = true; }
    CHECK(locked);
    bool unknown = false;
    ks.unlock_with_passphrase("hunter2");
    try { (void)custody::sign_transfer(ks, b, a, 1, 0, 0, n.chain_id()); } catch (const std::exception&) { unknown = true; }
    CHECK(unknown);
  }
  return bctest::summary();
}
