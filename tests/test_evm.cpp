#include <string>

#include "blockchainer/interop/evm.hpp"
#include "blockchainer/interop/rlp.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::interop;

namespace {
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
std::vector<std::uint8_t> unhex(const std::string& s) {
  std::vector<std::uint8_t> out;
  for (std::size_t i = 0; i + 1 < s.size(); i += 2)
    out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
  return out;
}
evm::Address addr_from_hex(const std::string& h) {
  auto b = unhex(h);
  evm::Address a{};
  std::copy(b.begin(), b.end(), a.begin());
  return a;
}
}  // namespace

int main() {
  init();

  std::printf("== EIP-55 checksum (spec example) ==\n");
  CHECK(evm::to_checksum_address(addr_from_hex("5aaeb6053f3e94c9b9a09f33669435e7ef1beaed")) ==
        "0x5aAeb6053F3E94C9b9A09f33669435E7Ef1BeAed");

  std::printf("== address from a known secret key ==\n");
  // Well-known Hardhat/Anvil account #0.
  auto sk = unhex("ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80");
  evm::Address a0 = evm::address_from_secret(view(sk));
  CHECK(evm::to_checksum_address(a0) == "0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266");

  std::printf("== legacy tx sign → recover sender (self-consistency) ==\n");
  {
    evm::LegacyTx tx;
    tx.nonce = 0;
    tx.gas_price = static_cast<codec::u128>(20000000000ull);  // 20 gwei
    tx.gas_limit = 21000;
    tx.to = addr_from_hex("3535353535353535353535353535353535353535");
    tx.value = static_cast<codec::u128>(1000000000000000000ull);  // 1 ether
    tx.chain_id = 1;

    evm::SignedTx st = evm::sign(tx, view(sk));
    CHECK(st.r.size() == 32 && st.s.size() == 32);
    CHECK(st.v == 37 || st.v == 38);  // 35 + 2*1 + recovery_id(0|1)

    // The raw signed tx is a 9-field RLP list.
    rlp::Item decoded = rlp::decode(view(st.raw));
    CHECK(decoded.is_list());
    CHECK(decoded.items.size() == 9);

    // ecrecover returns the signer.
    evm::Address signer = evm::recover_sender(tx, st);
    CHECK(signer == a0);

    // Independent signature verification against the signing hash.
    std::vector<std::uint8_t> rs = st.r;
    rs.insert(rs.end(), st.s.begin(), st.s.end());
    crypto::Hash h = evm::signing_hash(tx);
    auto pk = crypto::secp256k1_pubkey(view(sk), false);
    CHECK(crypto::secp256k1_verify(codec::ByteView(h.data(), 32), view(rs), view(pk)));
  }

  std::printf("== contract-creation tx (to = empty) signs + recovers ==\n");
  {
    evm::LegacyTx tx;
    tx.nonce = 5;
    tx.gas_price = static_cast<codec::u128>(1000000000ull);
    tx.gas_limit = 100000;
    tx.to = std::nullopt;  // creation
    tx.value = 0;
    tx.data = unhex("6000600055");  // tiny init code
    tx.chain_id = 11155111;          // sepolia
    evm::SignedTx st = evm::sign(tx, view(sk));
    CHECK(evm::recover_sender(tx, st) == evm::address_from_secret(view(sk)));
  }

  return bctest::summary();
}
