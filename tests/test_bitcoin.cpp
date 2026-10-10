#include <string>

#include "aviary/crypto.hpp"
#include "aviary/interop/bitcoin.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::interop;

namespace {
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
std::vector<std::uint8_t> unhex(const std::string& s) {
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}
std::string hex(const std::vector<std::uint8_t>& b) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s; for (std::uint8_t x : b) { s.push_back(k[x >> 4]); s.push_back(k[x & 0xF]); }
  return s;
}
}  // namespace

int main() {
  init();

  std::printf("== hashes ==\n");
  CHECK(bitcoin::hash160(view(unhex("00"))).size() == 20);
  CHECK(bitcoin::hash256(view(unhex("00"))).size() == 32);

  std::printf("== bech32 SegWit (BIP173 test vector) ==\n");
  // Witness v0 program 751e…3bd6 → the canonical BIP173 P2WPKH address.
  CHECK(bitcoin::bech32_segwit_encode("bc", 0, unhex("751e76e8199196d454941c45d1b3a323f1433bd6")) ==
        "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");

  std::printf("== hash160(compressed G) is that program ==\n");
  {
    std::vector<std::uint8_t> sk(32, 0);
    sk[31] = 1;  // privkey = 1 → pubkey = generator point
    auto pk = crypto::secp256k1_pubkey(view(sk), /*compressed=*/true);
    CHECK(hex(bitcoin::hash160(view(pk))) == "751e76e8199196d454941c45d1b3a323f1433bd6");
    // …so the derived P2WPKH address matches the BIP173 vector.
    CHECK(bitcoin::p2wpkh_address_from_secret(view(sk)) ==
          "bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kv8f3t4");
  }

  std::printf("== Base58Check round-trip + P2PKH ==\n");
  {
    auto payload = unhex("751e76e8199196d454941c45d1b3a323f1433bd6");
    std::string addr = bitcoin::base58check_encode(0x00, payload);
    auto [ver, body] = bitcoin::base58check_decode(addr);
    CHECK(ver == 0x00);
    CHECK(body == payload);

    std::vector<std::uint8_t> sk(32, 0);
    sk[31] = 1;
    auto pk = crypto::secp256k1_pubkey(view(sk), true);
    std::string p2pkh = bitcoin::p2pkh_address(view(pk));
    CHECK(!p2pkh.empty() && p2pkh[0] == '1');  // mainnet P2PKH starts with '1'
  }

  std::printf("== Base58Check rejects a corrupted checksum ==\n");
  {
    std::string addr = bitcoin::base58check_encode(0x00, unhex("751e76e8199196d454941c45d1b3a323f1433bd6"));
    addr.back() = (addr.back() == 'A') ? 'B' : 'A';  // flip last char
    CHECK_THROWS(bitcoin::base58check_decode(addr));
  }

  std::printf("== BIP143 P2WPKH sighash + signature (spec worked example) ==\n");
  {
    bitcoin::Tx tx;
    tx.version = 1;
    tx.locktime = 0x11;
    tx.inputs.push_back({{unhex("fff7f7881a8099afa6940d42d1e7f6362bec38171ea3edf433541db4e4ad969f"), 0}, 0xffffffee});
    tx.inputs.push_back({{unhex("ef51e1b804cc89d182d279655c3aa89e815b1b309fe287d9b2b55d57b90ec68a"), 1}, 0xffffffff});
    tx.outputs.push_back({0x06b22c20ull, unhex("76a9148280b37df378db99f66f85c95a783a76ac7a6d5988ac")});
    tx.outputs.push_back({0x0d519390ull, unhex("76a9143bde42dbee7e4dbe6a21b2d50ce2f0167faa815988ac")});

    auto sk = unhex("619c335025c7f4012e556c2a58b2506e30b8511b53ade95ea316fd8c3286feb9");
    auto pubkey = crypto::secp256k1_pubkey(view(sk), /*compressed=*/true);
    CHECK(hex(pubkey) == "025476c2e83188368da1ff3e292e7acafcdb3566bb0ad253f62fc70f07aeee6357");
    auto pkh = bitcoin::hash160(view(pubkey));
    CHECK(hex(pkh) == "1d0f172a0ecb48aee1be1f2687d2963ae33f71a1");

    auto sighash = bitcoin::bip143_sighash(tx, 1, bitcoin::p2wpkh_script_code(pkh), 600000000ull, 1);
    CHECK(hex(sighash) == "c37af31116d1b27caf68aae9e3ac82f1477929014d5b917657d0eb49478cb670");

    // Deterministic (RFC6979) signature matches the spec's witness signature.
    auto sig = bitcoin::sign_p2wpkh_input(tx, 1, 600000000ull, view(sk), pkh, 1);
    CHECK(hex(sig) ==
          "304402203609e17b84f6a7d30c80bfa610b5b4542f32a8a0d5447a12fb1366d7f01cc44a"
          "0220573a954c4518331561406f90300e8f3358f51928d43c212a8caed02de67eebee01");
  }

  std::printf("== single-input P2WPKH: sign + serialize round-trip ==\n");
  {
    std::vector<std::uint8_t> sk(32, 0);
    sk[31] = 7;
    auto pubkey = crypto::secp256k1_pubkey(view(sk), true);
    auto pkh = bitcoin::hash160(view(pubkey));

    bitcoin::Tx tx;
    tx.version = 2;
    tx.inputs.push_back({{std::vector<std::uint8_t>(32, 0xab), 0}, 0xffffffff});
    tx.outputs.push_back({50000, bitcoin::p2wpkh_script_pubkey(pkh)});

    auto sighash = bitcoin::bip143_sighash(tx, 0, bitcoin::p2wpkh_script_code(pkh), 100000, 1);
    auto rs = crypto::secp256k1_sign(view(sighash), view(sk));
    CHECK(crypto::secp256k1_verify(view(sighash),
                                   codec::ByteView(rs.data(), 64), view(pubkey)));

    auto sig = bitcoin::sign_p2wpkh_input(tx, 0, 100000, view(sk), pkh, 1);
    auto raw = bitcoin::serialize_segwit(tx, {{sig, pubkey}});
    CHECK(raw[0] == 0x02 && raw[4] == 0x00 && raw[5] == 0x01);  // version + segwit marker/flag
    CHECK(raw.size() > 60);
  }

  return bctest::summary();
}
