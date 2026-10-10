#include <cstdint>
#include <string>
#include <vector>

#include "aviary/codec.hpp"
#include "check.hpp"

using namespace aviary::codec;

namespace {

std::string hex(const Bytes& b) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string s;
  for (std::uint8_t x : b) {
    s.push_back(kHex[x >> 4]);
    s.push_back(kHex[x & 0xF]);
  }
  return s;
}

Bytes unhex(const std::string& s) {
  Bytes out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2) {
    out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  }
  return out;
}

ByteView view(const Bytes& b) { return ByteView(b.data(), b.size()); }

}  // namespace

int main() {
  std::printf("== fixed-width integers ==\n");
  {
    CodecWriter w;
    w.write_u8(0);
    w.write_u8(255);
    w.write_u16(0x0102);
    w.write_u32(0x01020304);
    w.write_u64(0x0102030405060708ull);
    CodecReader r(view(w.bytes()));
    CHECK(r.read_u8() == 0);
    CHECK(r.read_u8() == 255);
    CHECK(r.read_u16() == 0x0102);
    CHECK(r.read_u32() == 0x01020304u);
    CHECK(r.read_u64() == 0x0102030405060708ull);
    r.assert_fully_consumed();
  }
  {
    CodecWriter w;
    w.write_u16(0x0102);
    CHECK(hex(w.bytes()) == "0102");
  }
  {
    CodecWriter w;
    w.write_u32(0x01020304);
    CHECK(hex(w.bytes()) == "01020304");
  }
  // u64 high-bit-set rejected on read.
  {
    Bytes b = unhex("ffffffffffffffff");
    CodecReader r(view(b));
    CHECK_THROWS(r.read_u64());
  }
  // write_u64 rejects > 2^63-1.
  CHECK_THROWS(CodecWriter().write_u64(static_cast<std::uint64_t>(INT64_MAX) + 1));

  std::printf("== u128 values ==\n");
  {
    CodecWriter w;
    w.write_u128(static_cast<u128>(1));
    CHECK(hex(w.bytes()) == "00000000000000000000000000000001");
    CodecReader r(view(w.bytes()));
    CHECK(r.read_u128() == static_cast<u128>(1));
  }
  {
    // round-trip max
    u128 max = ~static_cast<u128>(0);
    CodecWriter w;
    w.write_u128(max);
    CHECK(w.bytes().size() == 16);
    CodecReader r(view(w.bytes()));
    CHECK(r.read_u128() == max);
  }

  std::printf("== bytes ==\n");
  {
    CodecWriter w;
    Bytes payload = unhex("deadbeef");
    w.write_bytes(view(payload));
    CHECK(hex(w.bytes()) == "00000004deadbeef");
    CodecReader r(view(w.bytes()));
    CHECK(hex(r.read_bytes()) == "deadbeef");
  }
  {
    CodecWriter w;
    Bytes h = unhex("0011223344556677");
    w.write_fixed_bytes(view(h), 8);
    CHECK(hex(w.bytes()) == "0011223344556677");
    CodecReader r(view(w.bytes()));
    CHECK(hex(r.read_fixed_bytes(8)) == "0011223344556677");
  }
  CHECK_THROWS(CodecWriter().write_fixed_bytes(view(unhex("00")), 8));
  // truncated input
  {
    Bytes b = unhex("0000000400ff");
    CodecReader r(view(b));
    CHECK_THROWS(r.read_bytes());
  }

  std::printf("== biguint (minimal big-endian) ==\n");
  {
    CodecWriter w;
    w.write_biguint(ByteView{});  // zero
    CHECK(hex(w.bytes()) == "00000000");
  }
  {
    CodecWriter w;
    Bytes m = unhex("0100");  // 256
    w.write_biguint(view(m));
    CHECK(hex(w.bytes()) == "000000020100");
  }
  // non-canonical leading zero rejected on read
  {
    Bytes b = unhex("000000020001");
    CodecReader r(view(b));
    CHECK_THROWS(r.read_biguint());
  }
  // non-minimal magnitude rejected on write
  CHECK_THROWS(CodecWriter().write_biguint(view(unhex("0001"))));

  std::printf("== lists ==\n");
  {
    CodecWriter w;
    std::vector<std::uint32_t> items = {1, 2, 3, 1000};
    w.write_list(items, [](CodecWriter& w, std::uint32_t v) { w.write_u32(v); });
    CodecReader r(view(w.bytes()));
    auto back = r.read_list<std::uint32_t>([](CodecReader& r) { return r.read_u32(); });
    CHECK(back == items);
    r.assert_fully_consumed();
  }
  // absurd declared count rejected without allocating
  {
    Bytes b = unhex("00ffffff");
    CodecReader r(view(b));
    CHECK_THROWS(r.read_list<std::uint8_t>([](CodecReader& r) { return r.read_u8(); }));
  }

  std::printf("== canonical-form enforcement ==\n");
  // trailing bytes
  {
    Bytes b = unhex("05ff");
    CodecReader r(view(b));
    (void)r.read_u8();
    CHECK_THROWS(r.assert_fully_consumed());
  }
  // non-canonical bool
  {
    Bytes b = unhex("02");
    CodecReader r(view(b));
    CHECK_THROWS(r.read_bool());
  }

  std::printf("== golden vector (drift guard) ==\n");
  {
    CodecWriter w;
    w.write_u8(1);                              // version
    w.write_u64(42);                            // height
    w.write_u128(static_cast<u128>(1000000));   // amount
    Bytes payload = unhex("cafebabe");
    w.write_bytes(view(payload));               // payload
    Bytes exp = unhex("010001");                // 65537 magnitude
    w.write_biguint(view(exp));                 // exponent
    w.write_bool(true);
    CHECK(hex(w.bytes()) ==
          "01"
          "000000000000002a"
          "000000000000000000000000000f4240"
          "00000004cafebabe"
          "00000003010001"
          "01");
  }

  return bctest::summary();
}
