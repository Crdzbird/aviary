#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

/// The crypto seam (ARCHITECTURE §3.2). Wraps cryptolib's C ABI in
/// domain-separated, intent-named C++ operations returning real C++ types, so
/// the C shape (lengths, out-pointers, buffer frees) never leaks past here.
/// Consensus and ledger code depend on this seam, never on cryptolib directly.
namespace blockchainer::crypto {

/// A 256-bit digest — the chain's canonical hash width.
using Hash = std::array<std::uint8_t, 32>;

/// Domain-separation tags (docs/CODEC_SPEC.md). A 1-byte prefix so a leaf can
/// never collide with an internal node or a header.
enum class Domain : std::uint8_t {
  kLeaf = 0x00,
  kNode = 0x01,
  kHeader = 0x02,
  kTxId = 0x03,
  kValidatorSet = 0x04,  ///< commitment to a validator set (distinct from a header)
};

/// BLAKE3-256 over [data, data+len). The chain's primary hash primitive.
/// Throws std::runtime_error if the backend reports failure.
Hash blake3_256(const std::uint8_t* data, std::size_t len);

/// Convenience overload over a byte vector.
Hash blake3_256(const std::vector<std::uint8_t>& data);

/// Domain-separated hash: BLAKE3-256( tag ‖ data ).
Hash hash_tagged(Domain tag, const std::vector<std::uint8_t>& data);

/// Internal Merkle node hash: BLAKE3-256( 0x01 ‖ left ‖ right ).
Hash hash_node(const Hash& left, const Hash& right);

/// All-zero digest (used as the conventional empty/sentinel hash).
Hash zero_hash();

// ── Signatures (Ed25519; ARCHITECTURE §3) ────────────────────────────────────

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

struct KeyPair {
  Bytes public_key;  ///< 32 bytes
  Bytes secret_key;  ///< 64 bytes
};

/// Fresh Ed25519 keypair.
KeyPair ed25519_keygen();

/// Deterministic Ed25519 keypair from a 32-byte seed.
KeyPair ed25519_keygen_from_seed(ByteView seed);

/// Ed25519 signature (64 bytes) over [msg]. Throws on backend failure.
Bytes ed25519_sign(ByteView msg, ByteView secret_key);

/// Verifies an Ed25519 signature. Never throws — returns false on any failure.
bool ed25519_verify(ByteView msg, ByteView sig, ByteView public_key);

/// Account address derived from a public key: BLAKE3-256(public_key).
Hash address_of(ByteView public_key);

// ── Hybrid signature: Ed25519 + ML-DSA-65 (post-quantum tx path) ─────────────
// Keys and signatures are concatenations of the classical then PQ parts
// (ed25519_part ‖ ml_dsa_part). BOTH parts must verify — a "harvest now, forge
// later" quantum adversary that breaks Ed25519 still faces ML-DSA, and a
// downgrade that strips the PQ part fails verification.

/// Hybrid keypair. public_key = ed_pub(32) ‖ mldsa_pub; secret_key = ed_sec ‖ mldsa_sec.
KeyPair hybrid_sig_keygen();

/// Hybrid signature over [msg]: ed_sig(64) ‖ mldsa_sig. Throws on backend failure.
Bytes hybrid_sig_sign(ByteView msg, ByteView secret_key);

/// Verifies a hybrid signature (both parts must pass). Never throws.
bool hybrid_sig_verify(ByteView msg, ByteView sig, ByteView public_key);

// ── BLS12-381 (consensus votes; ARCHITECTURE §7) ─────────────────────────────

/// Deterministic BLS keypair from input key material (>= 32 bytes).
/// secret_key = 32 bytes, public_key = 48 bytes.
KeyPair bls_keygen_from_ikm(ByteView ikm);

/// BLS signature (96-byte compressed G2) over [msg]. Throws on backend failure.
Bytes bls_sign(ByteView msg, ByteView secret_key);

/// Verifies a single BLS signature. Never throws.
bool bls_verify(ByteView msg, ByteView sig, ByteView public_key);

/// Aggregates N BLS signatures (each 96 bytes) into one. Throws if empty.
Bytes bls_aggregate(const std::vector<Bytes>& sigs);

/// Verifies an aggregate signature over N (message, public key) pairs. For BFT
/// votes the messages are identical (the same block id under many keys) — pass
/// the message repeated. Never throws.
bool bls_aggregate_verify(const std::vector<Bytes>& msgs,
                          const std::vector<Bytes>& pubkeys, ByteView agg_sig);

// ── Hybrid KEM + SecretStream (secure P2P channel; ARCHITECTURE §9) ──────────

/// BLAKE3 key derivation with a domain-separation context string. 32-byte key.
Bytes derive_key(const std::string& context, ByteView ikm);

struct KemEncaps {
  Bytes ciphertext;
  Bytes shared_secret;  ///< 32 bytes
};

/// Hybrid KEM (X25519 + ML-KEM-768) keypair (concatenated layout). Secure while
/// either the classical or the post-quantum part is unbroken.
KeyPair hybrid_kem_keygen();

/// Encapsulate to [public_key]: returns ciphertext + 32-byte shared secret.
KemEncaps hybrid_kem_encapsulate(ByteView public_key);

/// Decapsulate [ciphertext] with [secret_key]: returns the 32-byte shared secret.
Bytes hybrid_kem_decapsulate(ByteView ciphertext, ByteView secret_key);

/// Forward-secure stream encryptor (libsodium SecretStream). RAII over the
/// native handle; movable, non-copyable.
class StreamEncryptor {
 public:
  explicit StreamEncryptor(ByteView key32);
  ~StreamEncryptor();
  StreamEncryptor(StreamEncryptor&&) noexcept;
  StreamEncryptor& operator=(StreamEncryptor&&) noexcept;
  StreamEncryptor(const StreamEncryptor&) = delete;
  StreamEncryptor& operator=(const StreamEncryptor&) = delete;

  /// 24-byte stream header; must reach the decryptor before any frame.
  const Bytes& header() const { return header_; }
  /// Encrypt one frame. [final_chunk] marks the end of the stream.
  Bytes push(ByteView plaintext, bool final_chunk = false);

 private:
  void* h_ = nullptr;
  Bytes header_;
};

// ── Interop primitives (EVM / Bitcoin / Solana; ARCHITECTURE §11) ────────────

/// SHA-256 (Bitcoin, and general use).
Hash sha256(ByteView msg);

/// Keccak-256 (original Keccak padding — Ethereum). NOT NIST SHA3-256.
Hash keccak256(ByteView msg);

/// RIPEMD-160 (20-byte output). Bitcoin HASH160(x) = ripemd160(sha256(x)).
Bytes ripemd160(ByteView msg);

/// secp256k1 keypair: sk = 32 bytes, pk = 65 bytes uncompressed (0x04‖X‖Y).
KeyPair secp256k1_keygen();

/// Public key from a 32-byte secret key. compressed → 33 bytes, else 65.
Bytes secp256k1_pubkey(ByteView secret_key, bool compressed);

/// ECDSA sign a 32-byte digest (RFC6979, low-S). Returns 65 bytes:
/// r(32) ‖ s(32) ‖ recovery_id(1). Throws on failure.
Bytes secp256k1_sign(ByteView digest32, ByteView secret_key);

/// Verify a secp256k1 signature. [sig] is 64 (r‖s) or 65 bytes; [pubkey] 33/65.
bool secp256k1_verify(ByteView digest32, ByteView sig, ByteView pubkey);

/// Recover the 65-byte uncompressed public key from a 32-byte digest and a
/// 65-byte recoverable signature (r‖s‖recovery_id). Ethereum ecrecover.
Bytes secp256k1_recover(ByteView digest32, ByteView sig65);

/// Forward-secure stream decryptor. Throws on authentication failure.
class StreamDecryptor {
 public:
  StreamDecryptor(ByteView key32, ByteView header24);
  ~StreamDecryptor();
  StreamDecryptor(StreamDecryptor&&) noexcept;
  StreamDecryptor& operator=(StreamDecryptor&&) noexcept;
  StreamDecryptor(const StreamDecryptor&) = delete;
  StreamDecryptor& operator=(const StreamDecryptor&) = delete;

  /// Decrypt one frame; sets [*is_final] if non-null. Throws std::runtime_error
  /// on tamper / wrong order / truncation.
  Bytes pull(ByteView ciphertext, bool* is_final = nullptr);

 private:
  void* h_ = nullptr;
};

/// Lowercase hex encoding of a hash (no prefix). Test/debug helper.
std::string to_hex(const Hash& h);

}  // namespace blockchainer::crypto
