#include <string>

#include "aviary/interop/encoding.hpp"
#include "check.hpp"

using namespace aviary::interop::encoding;

namespace {
Bytes str(const char* s) { Bytes b; for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s)); return b; }
std::string hex(const Bytes& b) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s; for (std::uint8_t x : b) { s.push_back(k[x >> 4]); s.push_back(k[x & 0xF]); }
  return s;
}
}  // namespace

int main() {
  std::printf("== base64 (RFC 4648 vectors) ==\n");
  CHECK(base64_encode(str("")) == "");
  CHECK(base64_encode(str("f")) == "Zg==");
  CHECK(base64_encode(str("fo")) == "Zm8=");
  CHECK(base64_encode(str("foo")) == "Zm9v");
  CHECK(base64_encode(str("foob")) == "Zm9vYg==");
  CHECK(base64_encode(str("fooba")) == "Zm9vYmE=");
  CHECK(base64_encode(str("foobar")) == "Zm9vYmFy");
  CHECK(base64_decode("Zm9vYmFy") == str("foobar"));

  std::printf("== base58 ==\n");
  // 32 zero bytes → the Solana System Program id (32 '1's).
  CHECK(base58_encode(Bytes(32, 0)) == std::string(32, '1'));
  CHECK(base58_decode(std::string(32, '1')) == Bytes(32, 0));
  CHECK(base58_encode(Bytes{}) == "");
  // Leading zero bytes map to leading '1's; trailing 0x01 → '2'.
  CHECK(base58_encode(Bytes{0x00, 0x00, 0x01}) == "112");
  // Round-trip a non-trivial value.
  {
    Bytes data = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    CHECK(base58_decode(base58_encode(data)) == data);
  }
  CHECK_THROWS(base58_decode("0OIl"));  // characters not in the alphabet

  std::printf("== compact-u16 (Solana shortvec) ==\n");
  {
    auto enc = [](std::uint16_t v) { Bytes b; compact_u16_encode(v, b); return hex(b); };
    CHECK(enc(0) == "00");
    CHECK(enc(127) == "7f");
    CHECK(enc(128) == "8001");
    CHECK(enc(16384) == "808001");
    CHECK(enc(65535) == "ffff03");
    // Round-trip.
    for (std::uint16_t v : {std::uint16_t(0), std::uint16_t(1), std::uint16_t(300),
                            std::uint16_t(16383), std::uint16_t(65535)}) {
      Bytes b;
      compact_u16_encode(v, b);
      std::size_t pos = 0;
      CHECK(compact_u16_decode(b, pos) == v);
      CHECK(pos == b.size());
    }
  }

  return bctest::summary();
}
