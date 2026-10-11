#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/interop/evm.hpp"

/// Minimal Ethereum ABI encoding/decoding for **static** types (address,
/// uintN, bool, bytes32) - enough to build contract calls like ERC-20
/// transfer/balanceOf and decode their returns (ARCHITECTURE section 11). Dynamic types
/// (string, bytes, arrays) are deferred - see docs/IMPROVEMENTS.md.
namespace aviary::interop::abi {

using Bytes = std::vector<std::uint8_t>;

/// A 32-byte ABI word. Left-pads a big-endian value into the low bytes.
Bytes word_from_bytes(codec::ByteView big_endian);  // big_endian.size() <= 32
Bytes encode_uint(codec::u128 v);
Bytes encode_address(const evm::Address& a);
Bytes encode_bool(bool v);
Bytes encode_bytes32(codec::ByteView b);  // exactly 32 bytes, right-padded? no: as-is

/// 4-byte function selector: keccak256(signature)[:4],
/// e.g. selector("transfer(address,uint256)").
Bytes selector(const std::string& signature);

/// Encode a function call: selector || concatenated 32-byte argument words.
Bytes encode_call(const std::string& signature, const std::vector<Bytes>& arg_words);

/// Decode a 32-byte word.
codec::u128 decode_uint(codec::ByteView word32);  // low 16 bytes (throws if higher bits set)
evm::Address decode_address(codec::ByteView word32);

}  // namespace aviary::interop::abi
