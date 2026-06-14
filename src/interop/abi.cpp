#include "blockchainer/interop/abi.hpp"

#include <stdexcept>

#include "blockchainer/crypto.hpp"

namespace blockchainer::interop::abi {

Bytes word_from_bytes(codec::ByteView big_endian) {
  if (big_endian.size() > 32) throw std::runtime_error("abi: value exceeds 32 bytes");
  Bytes w(32, 0);
  std::copy(big_endian.begin(), big_endian.end(),
            w.end() - static_cast<std::ptrdiff_t>(big_endian.size()));  // right-align
  return w;
}

Bytes encode_uint(codec::u128 v) {
  std::uint8_t be[16];
  for (int i = 15; i >= 0; --i) {
    be[i] = static_cast<std::uint8_t>(v & 0xFF);
    v >>= 8;
  }
  return word_from_bytes(codec::ByteView(be, 16));
}

Bytes encode_address(const evm::Address& a) {
  return word_from_bytes(codec::ByteView(a.data(), a.size()));
}

Bytes encode_bool(bool v) {
  Bytes w(32, 0);
  w[31] = v ? 1 : 0;
  return w;
}

Bytes encode_bytes32(codec::ByteView b) {
  if (b.size() != 32) throw std::runtime_error("abi: bytes32 must be 32 bytes");
  return Bytes(b.begin(), b.end());
}

Bytes selector(const std::string& signature) {
  Bytes sig(signature.begin(), signature.end());
  crypto::Hash h = crypto::keccak256(codec::ByteView(sig.data(), sig.size()));
  return Bytes(h.begin(), h.begin() + 4);
}

Bytes encode_call(const std::string& signature, const std::vector<Bytes>& arg_words) {
  Bytes out = selector(signature);
  for (const auto& w : arg_words) {
    if (w.size() != 32) throw std::runtime_error("abi: argument word must be 32 bytes");
    out.insert(out.end(), w.begin(), w.end());
  }
  return out;
}

codec::u128 decode_uint(codec::ByteView word32) {
  if (word32.size() != 32) throw std::runtime_error("abi: word must be 32 bytes");
  // The high 16 bytes must be zero to fit a u128.
  for (std::size_t i = 0; i < 16; ++i) {
    if (word32[i] != 0) throw std::runtime_error("abi: value exceeds u128");
  }
  codec::u128 v = 0;
  for (std::size_t i = 16; i < 32; ++i) v = (v << 8) | word32[i];
  return v;
}

evm::Address decode_address(codec::ByteView word32) {
  if (word32.size() != 32) throw std::runtime_error("abi: word must be 32 bytes");
  evm::Address a{};
  std::copy(word32.begin() + 12, word32.end(), a.begin());  // low 20 bytes
  return a;
}

}  // namespace blockchainer::interop::abi
