#pragma once

#include <cstdint>
#include <string>
#include <vector>

/// Encodings used at interop boundaries (ARCHITECTURE section 11): Base58 (Bitcoin/
/// Solana alphabet), Base64 (RFC 4648, for Solana RPC), and compact-u16
/// (Solana "shortvec" array-length encoding).
namespace aviary::interop::encoding {

using Bytes = std::vector<std::uint8_t>;

/// Base58 with the Bitcoin/Solana alphabet. Leading zero bytes -> leading '1's.
std::string base58_encode(const Bytes& bytes);
Bytes base58_decode(const std::string& s);  // throws on invalid char

/// Base64 standard alphabet with '=' padding (RFC 4648).
std::string base64_encode(const Bytes& bytes);
Bytes base64_decode(const std::string& s);

/// Solana shortvec: a u16 in 1-3 bytes, 7 bits/byte, high bit = continuation.
void compact_u16_encode(std::uint16_t value, Bytes& out);
/// Decode at [pos]; advances pos. Throws on malformed input.
std::uint16_t compact_u16_decode(const Bytes& in, std::size_t& pos);

}  // namespace aviary::interop::encoding
