#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "blockchainer/codec.hpp"

/// Bitcoin interop (ARCHITECTURE §11): hashes, Base58Check + Bech32 encodings,
/// and address derivation (P2PKH legacy + P2WPKH SegWit). Transaction signing
/// (BIP143) is deferred — see docs/IMPROVEMENTS.md.
namespace blockchainer::interop::bitcoin {

using Bytes = std::vector<std::uint8_t>;

/// HASH160(x) = ripemd160(sha256(x)) — 20 bytes.
Bytes hash160(codec::ByteView data);
/// HASH256(x) = sha256(sha256(x)) — 32 bytes.
Bytes hash256(codec::ByteView data);

/// Base58Check: base58( version ‖ payload ‖ sha256d(version‖payload)[:4] ).
std::string base58check_encode(std::uint8_t version, const Bytes& payload);
/// Decode + verify checksum. Returns (version, payload). Throws on bad checksum.
std::pair<std::uint8_t, Bytes> base58check_decode(const std::string& s);

/// Bech32 SegWit address (BIP173) for a witness program.
std::string bech32_segwit_encode(const std::string& hrp, std::uint8_t witness_version,
                                 const Bytes& program);

/// Legacy P2PKH address ("1…" on mainnet, version 0x00).
std::string p2pkh_address(codec::ByteView compressed_pubkey, std::uint8_t version = 0x00);
/// SegWit v0 P2WPKH address ("bc1q…" on mainnet).
std::string p2wpkh_address(codec::ByteView compressed_pubkey, const std::string& hrp = "bc");

/// Derive a compressed pubkey from a secret key and return its P2WPKH address.
std::string p2wpkh_address_from_secret(codec::ByteView secret_key, const std::string& hrp = "bc");

// ── Transactions (P2WPKH / BIP143) ───────────────────────────────────────────

struct OutPoint {
  Bytes txid;             ///< 32 bytes, serialization (internal) byte order
  std::uint32_t vout = 0;
};
struct TxIn {
  OutPoint prevout;
  std::uint32_t sequence = 0xffffffff;
};
struct TxOut {
  std::uint64_t value = 0;  ///< satoshis
  Bytes script_pubkey;
};
struct Tx {
  std::uint32_t version = 2;
  std::vector<TxIn> inputs;
  std::vector<TxOut> outputs;
  std::uint32_t locktime = 0;
};

/// P2WPKH scriptCode: 76a914{20-byte pubkey hash}88ac.
Bytes p2wpkh_script_code(const Bytes& pubkey_hash);
/// P2WPKH scriptPubKey (witness v0): 0014{20-byte pubkey hash}.
Bytes p2wpkh_script_pubkey(const Bytes& pubkey_hash);

/// BIP143 sighash for input [index] spending a P2WPKH output worth [amount].
Bytes bip143_sighash(const Tx& tx, std::size_t index, const Bytes& script_code,
                     std::uint64_t amount, std::uint32_t sighash_type = 1);

/// DER-encode a 64-byte r‖s signature (low-S assumed). No sighash-type byte.
Bytes der_encode_signature(const Bytes& rs64);

/// Sign a P2WPKH input; returns the witness signature (DER ‖ sighash_type byte).
Bytes sign_p2wpkh_input(const Tx& tx, std::size_t index, std::uint64_t amount,
                        codec::ByteView secret_key, const Bytes& pubkey_hash,
                        std::uint32_t sighash_type = 1);

/// Serialize a fully-signed SegWit tx, given per-input witness (signature, pubkey) pairs.
Bytes serialize_segwit(const Tx& tx, const std::vector<std::pair<Bytes, Bytes>>& witnesses);

}  // namespace blockchainer::interop::bitcoin
