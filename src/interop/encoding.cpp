#include "blockchainer/interop/encoding.hpp"

#include <array>
#include <stdexcept>

namespace blockchainer::interop::encoding {

namespace {
const char* kB58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::array<int, 256> b58_index() {
  std::array<int, 256> m{};
  m.fill(-1);
  for (int i = 0; i < 58; ++i) m[static_cast<unsigned char>(kB58[i])] = i;
  return m;
}

const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}  // namespace

std::string base58_encode(const Bytes& bytes) {
  std::size_t zeros = 0;
  while (zeros < bytes.size() && bytes[zeros] == 0) ++zeros;

  // Upper bound on size: log(256)/log(58) ≈ 1.38.
  std::vector<std::uint8_t> b58((bytes.size() - zeros) * 138 / 100 + 1, 0);
  std::size_t length = 0;
  for (std::size_t i = zeros; i < bytes.size(); ++i) {
    int carry = bytes[i];
    std::size_t j = 0;
    for (std::size_t k = b58.size(); k-- > 0;) {
      if (carry == 0 && j >= length) break;
      carry += 256 * b58[k];
      b58[k] = static_cast<std::uint8_t>(carry % 58);
      carry /= 58;
      ++j;
    }
    length = j;
  }

  std::size_t skip = b58.size() - length;
  std::string out(zeros, '1');
  for (std::size_t k = skip; k < b58.size(); ++k) out.push_back(kB58[b58[k]]);
  return out;
}

Bytes base58_decode(const std::string& s) {
  static const std::array<int, 256> kIndex = b58_index();
  std::size_t zeros = 0;
  while (zeros < s.size() && s[zeros] == '1') ++zeros;

  std::vector<std::uint8_t> b256(s.size() * 733 / 1000 + 1, 0);
  std::size_t length = 0;
  for (std::size_t i = zeros; i < s.size(); ++i) {
    int carry = kIndex[static_cast<unsigned char>(s[i])];
    if (carry < 0) throw std::runtime_error("base58: invalid character");
    std::size_t j = 0;
    for (std::size_t k = b256.size(); k-- > 0;) {
      if (carry == 0 && j >= length) break;
      carry += 58 * b256[k];
      b256[k] = static_cast<std::uint8_t>(carry % 256);
      carry /= 256;
      ++j;
    }
    length = j;
  }

  std::size_t skip = b256.size() - length;
  Bytes out(zeros, 0);
  for (std::size_t k = skip; k < b256.size(); ++k) out.push_back(b256[k]);
  return out;
}

std::string base64_encode(const Bytes& bytes) {
  std::string out;
  std::size_t i = 0;
  for (; i + 2 < bytes.size(); i += 3) {
    std::uint32_t n = (bytes[i] << 16) | (bytes[i + 1] << 8) | bytes[i + 2];
    out.push_back(kB64[(n >> 18) & 63]);
    out.push_back(kB64[(n >> 12) & 63]);
    out.push_back(kB64[(n >> 6) & 63]);
    out.push_back(kB64[n & 63]);
  }
  const std::size_t rem = bytes.size() - i;
  if (rem == 1) {
    std::uint32_t n = bytes[i] << 16;
    out.push_back(kB64[(n >> 18) & 63]);
    out.push_back(kB64[(n >> 12) & 63]);
    out += "==";
  } else if (rem == 2) {
    std::uint32_t n = (bytes[i] << 16) | (bytes[i + 1] << 8);
    out.push_back(kB64[(n >> 18) & 63]);
    out.push_back(kB64[(n >> 12) & 63]);
    out.push_back(kB64[(n >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

Bytes base64_decode(const std::string& s) {
  std::array<int, 256> idx{};
  idx.fill(-1);
  for (int i = 0; i < 64; ++i) idx[static_cast<unsigned char>(kB64[i])] = i;

  Bytes out;
  std::uint32_t buf = 0;
  int bits = 0;
  for (char c : s) {
    if (c == '=') break;
    int v = idx[static_cast<unsigned char>(c)];
    if (v < 0) throw std::runtime_error("base64: invalid character");
    buf = (buf << 6) | static_cast<std::uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<std::uint8_t>((buf >> bits) & 0xFF));
    }
  }
  return out;
}

void compact_u16_encode(std::uint16_t value, Bytes& out) {
  std::uint32_t v = value;
  while (true) {
    std::uint8_t byte = v & 0x7F;
    v >>= 7;
    if (v == 0) {
      out.push_back(byte);
      break;
    }
    out.push_back(byte | 0x80);
  }
}

std::uint16_t compact_u16_decode(const Bytes& in, std::size_t& pos) {
  std::uint32_t value = 0;
  for (int i = 0; i < 3; ++i) {
    if (pos >= in.size()) throw std::runtime_error("compact_u16: unexpected end");
    std::uint8_t byte = in[pos++];
    value |= static_cast<std::uint32_t>(byte & 0x7F) << (7 * i);
    if ((byte & 0x80) == 0) {
      if (value > 0xFFFF) throw std::runtime_error("compact_u16: overflow");
      return static_cast<std::uint16_t>(value);
    }
  }
  throw std::runtime_error("compact_u16: too long");
}

}  // namespace blockchainer::interop::encoding
