#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/interop/rlp.hpp"

/// EVM (Ethereum) interop: addresses and legacy (EIP-155) transaction signing
/// (ARCHITECTURE §11). All signing uses cryptolib's secp256k1; hashing uses
/// cryptolib's keccak256. Read/RPC layer is separate (json_rpc).
namespace blockchainer::interop::evm {

using crypto::Hash;
using Bytes = std::vector<std::uint8_t>;
using Address = std::array<std::uint8_t, 20>;

/// Address from a secp256k1 public key (65-byte uncompressed or 64-byte X‖Y):
/// last 20 bytes of keccak256(X‖Y).
Address address_from_pubkey(codec::ByteView pubkey);
Address address_from_secret(codec::ByteView secret_key);

/// EIP-55 mixed-case checksum address, "0x"-prefixed.
std::string to_checksum_address(const Address& a);

/// Lowercase "0x"-prefixed hex (no checksum).
std::string to_hex_address(const Address& a);

/// A legacy (type-0) transaction.
struct LegacyTx {
  std::uint64_t nonce = 0;
  codec::u128 gas_price = 0;
  std::uint64_t gas_limit = 0;
  std::optional<Address> to;  ///< nullopt = contract creation
  codec::u128 value = 0;
  Bytes data;
  std::uint64_t chain_id = 1;
};

/// RLP of the EIP-155 signing preimage:
/// [nonce, gasPrice, gas, to, value, data, chainId, 0, 0].
Bytes signing_rlp(const LegacyTx& tx);
/// keccak256 of signing_rlp — the digest secp256k1 signs.
Hash signing_hash(const LegacyTx& tx);

struct SignedTx {
  Bytes raw;        ///< RLP of the full signed tx (for eth_sendRawTransaction)
  Hash tx_hash{};   ///< keccak256(raw)
  std::uint64_t v = 0;
  Bytes r;          ///< 32 bytes
  Bytes s;          ///< 32 bytes
};

/// Sign [tx] with a secp256k1 secret key. v = recovery_id + 35 + 2*chain_id.
SignedTx sign(const LegacyTx& tx, codec::ByteView secret_key);

/// Recover the sender address from a signed tx (ecrecover).
Address recover_sender(const LegacyTx& tx, const SignedTx& signed_tx);

}  // namespace blockchainer::interop::evm
