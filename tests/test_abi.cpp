#include <string>

#include "aviary/interop/abi.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::interop;

namespace {
std::string hex(const std::vector<std::uint8_t>& b) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s; for (std::uint8_t x : b) { s.push_back(k[x >> 4]); s.push_back(k[x & 0xF]); }
  return s;
}
std::vector<std::uint8_t> unhex(const std::string& s) {
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

int main() {
  init();

  std::printf("== ERC-20 function selectors (keccak anchors) ==\n");
  CHECK(hex(abi::selector("transfer(address,uint256)")) == "a9059cbb");
  CHECK(hex(abi::selector("balanceOf(address)")) == "70a08231");
  CHECK(hex(abi::selector("approve(address,uint256)")) == "095ea7b3");
  CHECK(hex(abi::selector("transferFrom(address,address,uint256)")) == "23b872dd");

  std::printf("== word encoding ==\n");
  CHECK(hex(abi::encode_uint(0x1234)) ==
        "0000000000000000000000000000000000000000000000000000000000001234");
  CHECK(hex(abi::encode_bool(true)).substr(62) == "01");

  evm::Address a{};
  auto ab = unhex("f39fd6e51aad88f6f4ce6ab8827279cfffb92266");
  std::copy(ab.begin(), ab.end(), a.begin());
  CHECK(hex(abi::encode_address(a)) ==
        "000000000000000000000000f39fd6e51aad88f6f4ce6ab8827279cfffb92266");

  std::printf("== encode a full ERC-20 transfer call ==\n");
  {
    abi::Bytes call = abi::encode_call("transfer(address,uint256)",
                                  {abi::encode_address(a), abi::encode_uint(1000000)});
    CHECK(call.size() == 4 + 32 + 32);
    CHECK(hex(abi::Bytes(call.begin(), call.begin() + 4)) == "a9059cbb");
  }

  std::printf("== decode round-trips ==\n");
  {
    CHECK(abi::decode_uint(view(abi::encode_uint(123456789))) == 123456789);
    CHECK(abi::decode_address(view(abi::encode_address(a))) == a);
    // A value with bits above 128 doesn't fit a u128.
    abi::Bytes big(32, 0xFF);
    CHECK_THROWS(abi::decode_uint(view(big)));
  }

  return bctest::summary();
}
