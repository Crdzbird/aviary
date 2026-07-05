#include "blockchainer/keyring.hpp"

#include <stdexcept>
#include <utility>

#include "blockchainer/codec.hpp"
#include "cryptolib_c.h"

namespace blockchainer::custody {

namespace {
// Consume a CryptoBufferResult into Bytes; throws with the backend message on
// failure. Frees the native buffer / error string.
Bytes take(CryptoBufferResult r, const char* op) {
  if (r.buf.data == nullptr) {
    std::string msg = r.error ? r.error : std::string(op) + " failed";
    if (r.error) cryptolib_str_free(r.error);
    throw std::runtime_error("custody: " + msg);
  }
  Bytes out(r.buf.data, r.buf.data + r.buf.len);
  cryptolib_buffer_free(&r.buf);
  return out;
}

// Hex of an address, usable as NUL-terminated AAD (raw address bytes may contain
// zero bytes, which a C string would truncate).
std::string aad_for(const Hash& address) {
  static const char* kHex = "0123456789abcdef";
  std::string s;
  s.reserve(64);
  for (std::uint8_t b : address) {
    s.push_back(kHex[b >> 4]);
    s.push_back(kHex[b & 0x0F]);
  }
  return s;
}

codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

// ── Keyring ─────────────────────────────────────────────────────────────────

Keyring Keyring::create() {
  CryptoKeyringHandle h = cryptolib_keyring_create();
  if (h == nullptr) throw std::runtime_error("custody: keyring create failed");
  return Keyring(h);
}

Keyring::Keyring(Keyring&& o) noexcept : h_(o.h_) { o.h_ = nullptr; }
Keyring& Keyring::operator=(Keyring&& o) noexcept {
  if (this != &o) {
    if (h_) cryptolib_keyring_free(h_);
    h_ = o.h_;
    o.h_ = nullptr;
  }
  return *this;
}
Keyring::~Keyring() {
  if (h_) cryptolib_keyring_free(h_);
}

bool Keyring::add_device_slot(ByteView factor_key) {
  return cryptolib_keyring_add_device_slot(h_, factor_key.data(), factor_key.size()) == 1;
}

bool Keyring::add_passphrase_slot(std::string_view passphrase, bool sensitive) {
  const std::string p(passphrase);
  return cryptolib_keyring_add_passphrase_slot(h_, p.c_str(), sensitive ? 1 : 0) == 1;
}

std::size_t Keyring::slot_count() const { return cryptolib_keyring_slot_count(h_); }

bool Keyring::remove_slot(std::size_t index) {
  return cryptolib_keyring_remove_slot(h_, index) == 1;
}

Bytes Keyring::serialise() const {
  return take(cryptolib_keyring_serialise(h_), "keyring serialise");
}

Keyring Keyring::deserialise(ByteView blob) {
  char* err = nullptr;
  CryptoKeyringHandle h = cryptolib_keyring_deserialise(blob.data(), blob.size(), &err);
  if (h == nullptr) {
    std::string msg = err ? err : "keyring deserialise failed";
    if (err) cryptolib_str_free(err);
    throw std::runtime_error("custody: " + msg);
  }
  return Keyring(h);
}

Bytes Keyring::unlock_with_device(ByteView factor_key) const {
  return take(cryptolib_keyring_unlock_with_device(h_, factor_key.data(), factor_key.size()),
              "keyring unlock (device)");
}

Bytes Keyring::unlock_with_passphrase(std::string_view passphrase) const {
  const std::string p(passphrase);
  return take(cryptolib_keyring_unlock_with_passphrase(h_, p.c_str()),
              "keyring unlock (passphrase)");
}

// ── Vault (committing AEAD keyed by the master) ─────────────────────────────

Vault::Vault(ByteView master_key) : master_(master_key.begin(), master_key.end()) {}

SealedBlob Vault::seal(ByteView plaintext, std::string_view aad) const {
  const auto* aadp = reinterpret_cast<const std::uint8_t*>(aad.data());
  return take(cryptolib_committing_encrypt(plaintext.data(), plaintext.size(), master_.data(),
                                           master_.size(), aadp, aad.size()),
              "vault seal");
}

Bytes Vault::open(const SealedBlob& blob, std::string_view aad) const {
  const auto* aadp = reinterpret_cast<const std::uint8_t*>(aad.data());
  return take(cryptolib_committing_decrypt(blob.data(), blob.size(), master_.data(),
                                           master_.size(), aadp, aad.size()),
              "vault open");
}

// ── KeyStore ────────────────────────────────────────────────────────────────

KeyStore KeyStore::create_with_passphrase(std::string_view passphrase, bool sensitive) {
  Keyring kr = Keyring::create();
  if (!kr.add_passphrase_slot(passphrase, sensitive)) {
    throw std::runtime_error("custody: could not add passphrase slot");
  }
  Bytes master = kr.unlock_with_passphrase(passphrase);
  KeyStore ks(std::move(kr));
  ks.vault_.emplace(view(master));
  return ks;
}

Hash KeyStore::add_account(ByteView public_key, ByteView secret) {
  if (!vault_) throw std::runtime_error("custody: key store is locked");
  const Hash address = crypto::address_of(public_key);
  Entry e;
  e.public_key.assign(public_key.begin(), public_key.end());
  e.sealed = vault_->seal(secret, aad_for(address));
  entries_[address] = std::move(e);
  return address;
}

std::vector<Hash> KeyStore::addresses() const {
  std::vector<Hash> out;
  out.reserve(entries_.size());
  for (const auto& [addr, _] : entries_) out.push_back(addr);
  return out;
}

Bytes KeyStore::public_key(const Hash& address) const {
  auto it = entries_.find(address);
  return it == entries_.end() ? Bytes{} : it->second.public_key;
}

Bytes KeyStore::reveal_secret(const Hash& address) const {
  if (!vault_) throw std::runtime_error("custody: key store is locked");
  auto it = entries_.find(address);
  if (it == entries_.end()) throw std::runtime_error("custody: unknown address");
  return vault_->open(it->second.sealed, aad_for(address));
}

void KeyStore::unlock_with_passphrase(std::string_view passphrase) {
  Bytes master = keyring_.unlock_with_passphrase(passphrase);
  vault_.emplace(view(master));
}

void KeyStore::unlock_with_device(ByteView factor_key) {
  Bytes master = keyring_.unlock_with_device(factor_key);
  vault_.emplace(view(master));
}

Bytes KeyStore::serialise() const {
  const Bytes keyring_blob = keyring_.serialise();
  codec::CodecWriter w;
  w.write_u8(1);  // format version
  w.write_bytes(view(keyring_blob));
  w.write_u32(static_cast<std::uint32_t>(entries_.size()));
  for (const auto& [addr, e] : entries_) {  // map → deterministic, address-sorted
    w.write_fixed_bytes(codec::ByteView(addr.data(), addr.size()), 32);
    w.write_bytes(view(e.public_key));
    w.write_bytes(view(e.sealed));
  }
  return w.take();
}

KeyStore KeyStore::load(ByteView blob) {
  codec::CodecReader r(codec::ByteView(blob.data(), blob.size()));
  if (r.read_u8() != 1) throw std::runtime_error("custody: unsupported key-store version");
  const Bytes keyring_blob = r.read_bytes();
  KeyStore ks(Keyring::deserialise(view(keyring_blob)));
  const std::uint32_t n = r.read_u32();
  for (std::uint32_t i = 0; i < n; ++i) {
    Bytes addr_bytes = r.read_fixed_bytes(32);
    Hash address{};
    for (std::size_t j = 0; j < 32; ++j) address[j] = addr_bytes[j];
    Entry e;
    e.public_key = r.read_bytes();
    e.sealed = r.read_bytes();
    ks.entries_[address] = std::move(e);
  }
  r.assert_fully_consumed();
  return ks;  // locked (no vault)
}

}  // namespace blockchainer::custody
