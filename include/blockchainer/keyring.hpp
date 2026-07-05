#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "blockchainer/crypto.hpp"

/// Custodial key management (ARCHITECTURE §: Wallet / key custody). Thin, RAII
/// wrappers over cryptolib's Vault (envelope-encrypt secrets under a master key)
/// and Keyring (protect that master key behind device / passphrase slots), plus
/// a [KeyStore] that keeps account signing secrets encrypted at rest.
///
/// The point: a node never persists a plaintext signing key. Secrets live sealed
/// in a serialisable blob; unlocking (a passphrase, or a hardware factor key)
/// recovers the master key just long enough to seal/open them. Each secret is
/// bound to its account address as additional authenticated data, so a sealed
/// blob cannot be relocated to a different account.
namespace blockchainer::custody {

using crypto::ByteView;
using crypto::Hash;
using Bytes = crypto::Bytes;

/// A secret sealed by a [Vault] → a flat, persistable blob (nonce ‖ ciphertext ‖
/// MAC ‖ Ed25519 signature ‖ KDF salt).
using SealedBlob = Bytes;

/// Envelope-encryption master-key custody: one random master key wrapped by one
/// or more unlock slots. Move-only; the native handle is freed on destruction.
class Keyring {
 public:
  /// A fresh keyring with a random master key, no slots. Add at least one slot
  /// before serialising, or the master key becomes unrecoverable.
  static Keyring create();

  Keyring(Keyring&&) noexcept;
  Keyring& operator=(Keyring&&) noexcept;
  Keyring(const Keyring&) = delete;
  Keyring& operator=(const Keyring&) = delete;
  ~Keyring();

  /// Add a hardware-factor (device) slot from a >= 32-byte key. Returns false on
  /// failure. [sensitive] selects the stronger (slower) Argon2id preset.
  bool add_device_slot(ByteView factor_key);
  bool add_passphrase_slot(std::string_view passphrase, bool sensitive = false);

  std::size_t slot_count() const;
  bool remove_slot(std::size_t index);

  /// Serialise the envelope blob (contains no plaintext key).
  Bytes serialise() const;
  /// Parse an envelope blob into a new (locked) keyring. Throws on a bad blob.
  static Keyring deserialise(ByteView blob);

  /// Recover the 32-byte master key. Throws if the factor/passphrase is wrong.
  Bytes unlock_with_device(ByteView factor_key) const;
  Bytes unlock_with_passphrase(std::string_view passphrase) const;

 private:
  explicit Keyring(void* handle) : h_(handle) {}
  void* h_ = nullptr;  // CryptoKeyringHandle
};

/// A symmetric envelope-encryptor keyed by a 32-byte master key, using a
/// committing AEAD (commits to key + AAD → closes the partitioning-oracle /
/// key-confusion gap). Stateless given the key: a blob sealed now still opens
/// after the store is persisted and reloaded under the same recovered master.
class Vault {
 public:
  explicit Vault(ByteView master_key);

  /// Encrypt [plaintext] (with optional [aad] bound in). Throws on failure.
  SealedBlob seal(ByteView plaintext, std::string_view aad = "") const;
  /// Decrypt a [blob]; [aad] must match what was sealed. Throws on tamper /
  /// wrong key / wrong aad.
  Bytes open(const SealedBlob& blob, std::string_view aad = "") const;

 private:
  Bytes master_;
};

/// A custodial store for account signing secrets. Each secret is sealed by a
/// Vault whose master key is protected by a Keyring's slots. Persist it, reload
/// it locked, then unlock with a passphrase / device factor to reveal a secret
/// (e.g. to sign a transaction).
class KeyStore {
 public:
  /// A new store protected by [passphrase] (adds one passphrase slot) — created
  /// unlocked.
  static KeyStore create_with_passphrase(std::string_view passphrase, bool sensitive = false);

  KeyStore(KeyStore&&) noexcept = default;
  KeyStore& operator=(KeyStore&&) noexcept = default;

  /// Seal an account's [secret]; returns its address (BLAKE3(public_key)). The
  /// store must be unlocked. The public key is stored in the clear.
  Hash add_account(ByteView public_key, ByteView secret);

  bool has(const Hash& address) const { return entries_.count(address) != 0; }
  std::vector<Hash> addresses() const;
  /// The stored public key for [address] (empty if unknown).
  Bytes public_key(const Hash& address) const;
  /// Decrypt and return the sealed secret for [address]. The store must be
  /// unlocked. Throws if unknown or locked.
  Bytes reveal_secret(const Hash& address) const;

  bool unlocked() const { return vault_.has_value(); }
  void lock() { vault_.reset(); }
  void unlock_with_passphrase(std::string_view passphrase);
  void unlock_with_device(ByteView factor_key);

  /// Slot management (delegates to the keyring). Adding a passphrase/device slot
  /// requires no unlock — slots wrap the same master key.
  bool add_passphrase_slot(std::string_view passphrase, bool sensitive = false) {
    return keyring_.add_passphrase_slot(passphrase, sensitive);
  }
  bool add_device_slot(ByteView factor_key) { return keyring_.add_device_slot(factor_key); }
  std::size_t slot_count() const { return keyring_.slot_count(); }

  /// Serialise the whole store: the keyring envelope + every sealed entry. No
  /// plaintext secret is present.
  Bytes serialise() const;
  /// Load a serialised store. It comes back LOCKED — call unlock_* before
  /// revealing secrets. Throws on a malformed blob.
  static KeyStore load(ByteView blob);

 private:
  explicit KeyStore(Keyring keyring) : keyring_(std::move(keyring)) {}

  struct Entry {
    Bytes public_key;
    SealedBlob sealed;
  };

  Keyring keyring_;
  std::optional<Vault> vault_;  // present iff unlocked
  std::map<Hash, Entry> entries_;
};

}  // namespace blockchainer::custody
