#pragma once

#include <cstdint>
#include <vector>

#include "blockchainer/codec.hpp"

/// Ethereum RLP (Recursive Length Prefix) encoding — the serialization EVM uses
/// for transactions and more (ARCHITECTURE §11). An item is either a byte string
/// or a list of items. Distinct from blockchainer's own canonical codec; used
/// only at the EVM interop boundary.
namespace blockchainer::interop::rlp {

using Bytes = std::vector<std::uint8_t>;
using ByteView = codec::ByteView;

struct Item {
  enum class Kind { kString, kList };
  Kind kind = Kind::kString;
  Bytes bytes;               // valid when kind == kString
  std::vector<Item> items;   // valid when kind == kList

  static Item string(Bytes b) { return Item{Kind::kString, std::move(b), {}}; }
  static Item list(std::vector<Item> xs) { return Item{Kind::kList, {}, std::move(xs)}; }

  bool is_list() const { return kind == Kind::kList; }
};

/// Minimal big-endian byte string for an unsigned integer (empty for 0) — the
/// canonical RLP integer form (no leading zero bytes).
Bytes uint_bytes(std::uint64_t v);
Bytes uint_bytes_u128(codec::u128 v);

/// Encode an item to its RLP byte form.
Bytes encode(const Item& item);

/// Decode exactly one RLP item from [bytes]; rejects trailing bytes.
Item decode(ByteView bytes);

}  // namespace blockchainer::interop::rlp
