#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

/// Canonical, deterministic binary serialization - the determinism bedrock
/// (ARCHITECTURE section 2/section 4, docs/CODEC_SPEC.md). One value has exactly one valid
/// encoding: the writer only ever emits it; the reader rejects anything else
/// (trailing bytes, non-minimal biguint, out-of-range u64, malformed bool)
/// rather than normalizing it.
namespace aviary::codec {

/// 128-bit unsigned, used for value/amount fields (fixed 16-byte wire form).
using u128 = unsigned __int128;

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

/// Coarse decode ceilings guarding against allocation-DoS from adversarial
/// input. A declared length is always also checked against bytes remaining.
inline constexpr std::size_t kMaxFieldLength = 256u * 1024u * 1024u;
inline constexpr std::size_t kMaxListLength = 16u * 1024u * 1024u;

/// Thrown on malformed, truncated, or non-canonical input during decoding.
class CodecError : public std::runtime_error {
 public:
  explicit CodecError(const std::string& msg, std::size_t offset = 0)
      : std::runtime_error(offset == 0
                               ? "CodecError: " + msg
                               : "CodecError: " + msg + " (at byte " +
                                     std::to_string(offset) + ")"),
        offset(offset) {}
  std::size_t offset;
};

/// Deterministic canonical encoder.
class CodecWriter {
 public:
  void write_u8(std::uint8_t v) { out_.push_back(v); }
  void write_bool(bool v) { out_.push_back(v ? 1 : 0); }
  void write_u16(std::uint16_t v);
  void write_u32(std::uint32_t v);
  /// 64-bit unsigned; values must be < 2^63 (height/nonce/length range).
  void write_u64(std::uint64_t v);
  /// Fixed 16-byte big-endian value field.
  void write_u128(u128 v);
  /// Length-prefixed (u32) variable byte string.
  void write_bytes(ByteView bytes);
  /// Exactly expected_len bytes, no prefix (hashes, public keys).
  void write_fixed_bytes(ByteView bytes, std::size_t expected_len);
  /// Minimal big-endian unsigned integer (length-prefixed). [magnitude] must be
  /// big-endian with no leading zero byte; empty means zero.
  void write_biguint(ByteView magnitude);

  /// Count-prefixed list; encode_elem(writer, item) per element, in order.
  template <class T, class F>
  void write_list(const std::vector<T>& items, F&& encode_elem) {
    if (items.size() > kMaxListLength) {
      throw std::out_of_range("write_list: exceeds kMaxListLength");
    }
    write_u32(static_cast<std::uint32_t>(items.size()));
    for (const auto& item : items) encode_elem(*this, item);
  }

  const Bytes& bytes() const { return out_; }
  Bytes take() { return std::move(out_); }
  std::size_t size() const { return out_.size(); }

 private:
  Bytes out_;
};

/// Deterministic canonical decoder - the strict mirror of CodecWriter.
class CodecReader {
 public:
  explicit CodecReader(ByteView bytes) : data_(bytes) {}

  std::size_t position() const { return pos_; }
  std::size_t remaining() const { return data_.size() - pos_; }
  bool fully_consumed() const { return pos_ == data_.size(); }
  /// Rejects trailing bytes (non-canonical).
  void assert_fully_consumed() const;

  std::uint8_t read_u8();
  bool read_bool();
  std::uint16_t read_u16();
  std::uint32_t read_u32();
  std::uint64_t read_u64();
  u128 read_u128();
  Bytes read_bytes();
  Bytes read_fixed_bytes(std::size_t len);
  /// Returns the minimal big-endian magnitude (empty for zero); rejects a
  /// leading zero byte.
  Bytes read_biguint();

  /// Count-prefixed list; decode_elem(reader) per element.
  template <class T, class F>
  std::vector<T> read_list(F&& decode_elem) {
    const std::uint32_t count = read_u32();
    if (count > kMaxListLength) {
      throw CodecError("list count exceeds max", pos_);
    }
    if (count > remaining()) {
      throw CodecError("list count exceeds remaining bytes", pos_);
    }
    std::vector<T> v;
    v.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) v.push_back(decode_elem(*this));
    return v;
  }

 private:
  void need(std::size_t n) const;
  ByteView data_;
  std::size_t pos_ = 0;
};

}  // namespace aviary::codec
