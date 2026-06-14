#include "blockchainer/interop/rlp.hpp"

#include <stdexcept>

namespace blockchainer::interop::rlp {

namespace {
// Minimal big-endian bytes of a length (no leading zeros). Used for long forms.
Bytes be_minimal(std::uint64_t v) {
  Bytes b;
  while (v > 0) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    v >>= 8;
  }
  return {b.rbegin(), b.rend()};
}

void encode_length(Bytes& out, std::size_t len, std::uint8_t short_base,
                   std::uint8_t long_base) {
  if (len <= 55) {
    out.push_back(static_cast<std::uint8_t>(short_base + len));
  } else {
    Bytes lb = be_minimal(static_cast<std::uint64_t>(len));
    out.push_back(static_cast<std::uint8_t>(long_base + lb.size()));
    out.insert(out.end(), lb.begin(), lb.end());
  }
}

void encode_into(const Item& item, Bytes& out) {
  if (!item.is_list()) {
    const Bytes& s = item.bytes;
    if (s.size() == 1 && s[0] < 0x80) {
      out.push_back(s[0]);  // single low byte is its own encoding
    } else {
      encode_length(out, s.size(), 0x80, 0xb7);
      out.insert(out.end(), s.begin(), s.end());
    }
  } else {
    Bytes payload;
    for (const auto& child : item.items) encode_into(child, payload);
    encode_length(out, payload.size(), 0xc0, 0xf7);
    out.insert(out.end(), payload.begin(), payload.end());
  }
}

class Cursor {
 public:
  explicit Cursor(ByteView b) : b_(b) {}
  std::size_t remaining() const { return b_.size() - pos_; }
  std::size_t pos() const { return pos_; }

  std::uint8_t peek() const {
    if (pos_ >= b_.size()) throw std::runtime_error("rlp: unexpected end");
    return b_[pos_];
  }
  std::uint8_t take() {
    std::uint8_t v = peek();
    ++pos_;
    return v;
  }
  Bytes take_n(std::size_t n) {
    if (pos_ + n > b_.size()) throw std::runtime_error("rlp: unexpected end");
    Bytes out(b_.begin() + static_cast<std::ptrdiff_t>(pos_),
              b_.begin() + static_cast<std::ptrdiff_t>(pos_ + n));
    pos_ += n;
    return out;
  }
  std::uint64_t take_length(std::size_t len_of_len) {
    if (len_of_len == 0 || len_of_len > 8) throw std::runtime_error("rlp: bad length");
    Bytes lb = take_n(len_of_len);
    if (lb[0] == 0) throw std::runtime_error("rlp: non-canonical length");  // no leading zero
    std::uint64_t v = 0;
    for (std::uint8_t c : lb) v = (v << 8) | c;
    if (v <= 55) throw std::runtime_error("rlp: non-canonical (should be short form)");
    return v;
  }

 private:
  ByteView b_;
  std::size_t pos_ = 0;
};

Item decode_item(Cursor& c) {
  const std::uint8_t prefix = c.peek();
  if (prefix < 0x80) {
    return Item::string(Bytes{c.take()});  // single byte
  } else if (prefix <= 0xb7) {
    c.take();
    const std::size_t len = prefix - 0x80u;
    Bytes s = c.take_n(len);
    if (len == 1 && s[0] < 0x80) throw std::runtime_error("rlp: non-canonical single byte");
    return Item::string(std::move(s));
  } else if (prefix <= 0xbf) {
    c.take();
    const std::uint64_t len = c.take_length(prefix - 0xb7u);
    return Item::string(c.take_n(len));
  } else if (prefix <= 0xf7) {
    c.take();
    const std::size_t len = prefix - 0xc0u;
    const std::size_t end = c.pos() + len;
    std::vector<Item> items;
    while (c.pos() < end) items.push_back(decode_item(c));
    if (c.pos() != end) throw std::runtime_error("rlp: list overrun");
    return Item::list(std::move(items));
  } else {
    c.take();
    const std::uint64_t len = c.take_length(prefix - 0xf7u);
    const std::size_t end = c.pos() + len;
    std::vector<Item> items;
    while (c.pos() < end) items.push_back(decode_item(c));
    if (c.pos() != end) throw std::runtime_error("rlp: list overrun");
    return Item::list(std::move(items));
  }
}
}  // namespace

Bytes uint_bytes(std::uint64_t v) { return be_minimal(v); }

Bytes uint_bytes_u128(codec::u128 v) {
  Bytes b;
  while (v > 0) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    v >>= 8;
  }
  return {b.rbegin(), b.rend()};
}

Bytes encode(const Item& item) {
  Bytes out;
  encode_into(item, out);
  return out;
}

Item decode(ByteView bytes) {
  Cursor c(bytes);
  Item item = decode_item(c);
  if (c.remaining() != 0) throw std::runtime_error("rlp: trailing bytes");
  return item;
}

}  // namespace blockchainer::interop::rlp
