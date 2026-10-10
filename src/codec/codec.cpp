#include <stdexcept>
#include "aviary/codec.hpp"

namespace aviary::codec {

// ── Writer ───────────────────────────────────────────────────────────────────

void CodecWriter::write_u16(std::uint16_t v) {
  out_.push_back(static_cast<std::uint8_t>(v >> 8));
  out_.push_back(static_cast<std::uint8_t>(v));
}

void CodecWriter::write_u32(std::uint32_t v) {
  out_.push_back(static_cast<std::uint8_t>(v >> 24));
  out_.push_back(static_cast<std::uint8_t>(v >> 16));
  out_.push_back(static_cast<std::uint8_t>(v >> 8));
  out_.push_back(static_cast<std::uint8_t>(v));
}

void CodecWriter::write_u64(std::uint64_t v) {
  // Mirror the reader: values must fit in the non-negative signed range so the
  // wire form is unambiguous across languages that lack unsigned 64-bit ints.
  if (v > static_cast<std::uint64_t>(INT64_MAX)) {
    throw std::out_of_range("write_u64: value exceeds 2^63-1");
  }
  for (int shift = 56; shift >= 0; shift -= 8) {
    out_.push_back(static_cast<std::uint8_t>(v >> shift));
  }
}

void CodecWriter::write_u128(u128 v) {
  std::uint8_t buf[16];
  for (int i = 15; i >= 0; --i) {
    buf[i] = static_cast<std::uint8_t>(v & 0xFF);
    v >>= 8;
  }
  out_.insert(out_.end(), buf, buf + 16);
}

void CodecWriter::write_bytes(ByteView bytes) {
  if (bytes.size() > kMaxFieldLength) {
    throw std::out_of_range("write_bytes: exceeds kMaxFieldLength");
  }
  write_u32(static_cast<std::uint32_t>(bytes.size()));
  out_.insert(out_.end(), bytes.begin(), bytes.end());
}

void CodecWriter::write_fixed_bytes(ByteView bytes, std::size_t expected_len) {
  if (bytes.size() != expected_len) {
    throw std::invalid_argument("write_fixed_bytes: wrong length");
  }
  out_.insert(out_.end(), bytes.begin(), bytes.end());
}

void CodecWriter::write_biguint(ByteView magnitude) {
  // Caller supplies a minimal big-endian magnitude. Enforce no leading zero so
  // we never emit a non-canonical form.
  if (!magnitude.empty() && magnitude.front() == 0) {
    throw std::invalid_argument("write_biguint: non-minimal (leading zero)");
  }
  write_bytes(magnitude);
}

// ── Reader ───────────────────────────────────────────────────────────────────

void CodecReader::need(std::size_t n) const {
  if (pos_ + n > data_.size()) {
    throw CodecError("unexpected end of input", pos_);
  }
}

void CodecReader::assert_fully_consumed() const {
  if (pos_ != data_.size()) {
    throw CodecError("trailing bytes", pos_);
  }
}

std::uint8_t CodecReader::read_u8() {
  need(1);
  return data_[pos_++];
}

bool CodecReader::read_bool() {
  const std::uint8_t b = read_u8();
  if (b != 0 && b != 1) throw CodecError("non-canonical bool", pos_ - 1);
  return b == 1;
}

std::uint16_t CodecReader::read_u16() {
  need(2);
  std::uint16_t v = (static_cast<std::uint16_t>(data_[pos_]) << 8) |
                    static_cast<std::uint16_t>(data_[pos_ + 1]);
  pos_ += 2;
  return v;
}

std::uint32_t CodecReader::read_u32() {
  need(4);
  std::uint32_t v = (static_cast<std::uint32_t>(data_[pos_]) << 24) |
                    (static_cast<std::uint32_t>(data_[pos_ + 1]) << 16) |
                    (static_cast<std::uint32_t>(data_[pos_ + 2]) << 8) |
                    static_cast<std::uint32_t>(data_[pos_ + 3]);
  pos_ += 4;
  return v;
}

std::uint64_t CodecReader::read_u64() {
  need(8);
  std::uint64_t v = 0;
  for (std::size_t i = 0; i < 8; ++i) {
    v = (v << 8) | static_cast<std::uint64_t>(data_[pos_ + i]);
  }
  pos_ += 8;
  if (v > static_cast<std::uint64_t>(INT64_MAX)) {
    throw CodecError("u64 value exceeds 2^63-1", pos_ - 8);
  }
  return v;
}

u128 CodecReader::read_u128() {
  need(16);
  u128 v = 0;
  for (std::size_t i = 0; i < 16; ++i) {
    v = (v << 8) | static_cast<u128>(data_[pos_ + i]);
  }
  pos_ += 16;
  return v;
}

Bytes CodecReader::read_bytes() {
  const std::uint32_t len = read_u32();
  if (len > kMaxFieldLength) throw CodecError("byte field exceeds max", pos_);
  need(len);
  Bytes out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
            data_.begin() + static_cast<std::ptrdiff_t>(pos_ + len));
  pos_ += len;
  return out;
}

Bytes CodecReader::read_fixed_bytes(std::size_t len) {
  need(len);
  Bytes out(data_.begin() + static_cast<std::ptrdiff_t>(pos_),
            data_.begin() + static_cast<std::ptrdiff_t>(pos_ + len));
  pos_ += len;
  return out;
}

Bytes CodecReader::read_biguint() {
  const std::size_t start = pos_;
  Bytes b = read_bytes();
  if (!b.empty() && b.front() == 0) {
    throw CodecError("non-canonical biguint: leading zero byte", start);
  }
  return b;
}

}  // namespace aviary::codec
