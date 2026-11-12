#pragma once

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "aviary_c.h"

namespace cabi {

using Bytes = std::vector<std::uint8_t>;

/// The buffer of a result, or empty (and the error printed) when it failed.
inline Bytes take(AviaryBufferResult r) {
  if (r.error) {
    std::printf("  error: %s\n", r.error);
    aviary_str_free(r.error);
    return {};
  }
  Bytes out(r.buf.data, r.buf.data + r.buf.len);
  aviary_buffer_free(&r.buf);
  return out;
}

/// The error text of a result, or empty when it succeeded.
inline std::string take_error(AviaryBufferResult r) {
  std::string e = r.error ? r.error : "";
  if (r.error) aviary_str_free(r.error);
  if (r.buf.data) aviary_buffer_free(&r.buf);
  return e;
}

/// The text a result carries in its buffer (for the *_error functions).
inline std::string take_text(AviaryBufferResult r) {
  const Bytes b = take(r);
  return std::string(b.begin(), b.end());
}

inline std::uint32_t be32(const Bytes& b, std::size_t o) {
  return (std::uint32_t(b[o]) << 24) | (std::uint32_t(b[o + 1]) << 16) | (std::uint32_t(b[o + 2]) << 8) | b[o + 3];
}

inline std::uint64_t be64(const Bytes& b, std::size_t o) {
  return (std::uint64_t(be32(b, o)) << 32) | be32(b, o + 4);
}

/// Splits an account blob [pub_len:4][public][secret].
inline std::pair<Bytes, Bytes> split(const Bytes& b) {
  const std::uint32_t n = be32(b, 0);
  return {Bytes(b.begin() + 4, b.begin() + 4 + n), Bytes(b.begin() + 4 + n, b.end())};
}

/// Decodes a blob list [count u32]([len u32][bytes])*.
inline std::vector<Bytes> blobs(const Bytes& b) {
  std::vector<Bytes> out;
  std::size_t o = 4;
  const std::uint32_t n = be32(b, 0);
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint32_t l = be32(b, o);
    o += 4;
    out.emplace_back(b.begin() + static_cast<std::ptrdiff_t>(o), b.begin() + static_cast<std::ptrdiff_t>(o + l));
    o += l;
  }
  return out;
}

/// Encodes a blob list.
inline Bytes list(const std::vector<Bytes>& parts) {
  Bytes out;
  auto put32 = [&](std::uint32_t v) {
    for (int s = 24; s >= 0; s -= 8) out.push_back(std::uint8_t(v >> s));
  };
  put32(static_cast<std::uint32_t>(parts.size()));
  for (const auto& p : parts) {
    put32(static_cast<std::uint32_t>(p.size()));
    out.insert(out.end(), p.begin(), p.end());
  }
  return out;
}

/// A BLS validator key pair from a one byte seed: {public, secret}.
inline std::pair<Bytes, Bytes> bls(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return split(take(aviary_bls_keygen(ikm.data(), ikm.size())));
}

/// Lower case hex of [b].
inline std::string hex(const Bytes& b) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string s;
  for (std::uint8_t x : b) {
    s.push_back(kHex[x >> 4]);
    s.push_back(kHex[x & 0xF]);
  }
  return s;
}

}  // namespace cabi
