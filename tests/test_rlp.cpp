#include <string>

#include "aviary/interop/rlp.hpp"
#include "check.hpp"

using namespace aviary::interop::rlp;

namespace {
Bytes str(const char* s) { Bytes b; for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s)); return b; }
std::string hex(const Bytes& b) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s; for (std::uint8_t x : b) { s.push_back(k[x >> 4]); s.push_back(k[x & 0xF]); }
  return s;
}
ByteView view(const Bytes& b) { return ByteView(b.data(), b.size()); }
}  // namespace

int main() {
  std::printf("== string KATs (Ethereum yellow paper) ==\n");
  CHECK(hex(encode(Item::string(str("")))) == "80");
  CHECK(hex(encode(Item::string(str("dog")))) == "83646f67");
  CHECK(hex(encode(Item::string(Bytes{0x00}))) == "00");
  CHECK(hex(encode(Item::string(Bytes{0x0f}))) == "0f");
  CHECK(hex(encode(Item::string(Bytes{0x04, 0x00}))) == "820400");

  std::printf("== integer encodings ==\n");
  CHECK(hex(encode(Item::string(uint_bytes(0)))) == "80");      // 0 -> empty string
  CHECK(hex(encode(Item::string(uint_bytes(15)))) == "0f");
  CHECK(hex(encode(Item::string(uint_bytes(1024)))) == "820400");

  std::printf("== list KATs ==\n");
  CHECK(hex(encode(Item::list({}))) == "c0");
  CHECK(hex(encode(Item::list({Item::string(str("cat")), Item::string(str("dog"))}))) ==
        "c88363617483646f67");

  std::printf("== long string (>55 bytes) uses the b8 form ==\n");
  {
    Bytes long_str(56, 0x61);  // 56 x 'a'
    Bytes enc = encode(Item::string(long_str));
    CHECK(enc[0] == 0xb8);
    CHECK(enc[1] == 56);
    CHECK(enc.size() == 58);
  }

  std::printf("== decode round-trips ==\n");
  {
    Item nested = Item::list({
        Item::string(uint_bytes(9)),
        Item::string(uint_bytes(1024)),
        Item::list({Item::string(str("nested")), Item::string(Bytes{0x00})}),
    });
    Bytes enc = encode(nested);
    Item back = decode(view(enc));
    CHECK(hex(encode(back)) == hex(enc));
    CHECK(back.is_list());
    CHECK(back.items.size() == 3);
    CHECK(back.items[2].is_list());
  }

  std::printf("== canonical rejection ==\n");
  {
    // Trailing bytes.
    Bytes t = encode(Item::string(str("dog")));
    t.push_back(0x00);
    CHECK_THROWS(decode(view(t)));
    // Non-canonical single byte: 81 00 should have been just 00.
    CHECK_THROWS(decode(view(Bytes{0x81, 0x00})));
    // Truncated.
    CHECK_THROWS(decode(view(Bytes{0x83, 0x64, 0x6f})));  // says 3 bytes, only 2
  }

  return bctest::summary();
}
