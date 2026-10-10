// Generates the canonical txid / header_hash golden vectors from the C++ core,
// for fixed inputs. The Dart and Go bindings replay the same inputs through the
// C ABI and must reproduce these exactly. Run: ./build/print_vectors
#include <cstdint>
#include <cstdio>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/crypto.hpp"
#include "aviary/proofs.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"

using namespace aviary;

static void print_hash(const char* label, const Hash& h) {
  std::printf("%s ", label);
  std::printf("%s\n", crypto::to_hex(h).c_str());
}

static std::string hex(const Bytes& b) {
  static constexpr char kHex[] = "0123456789abcdef";
  std::string s;
  for (std::uint8_t x : b) { s.push_back(kHex[x >> 4]); s.push_back(kHex[x & 0xF]); }
  return s;
}

int main() {
  init();

  // Fixed transaction: version 1, chain 7, body "hello", author 32×0x11, sig 64×0x22.
  Transaction tx;
  tx.version = 1;
  tx.chain_id = 7;
  tx.body = Bytes{'h', 'e', 'l', 'l', 'o'};
  tx.author = Bytes(32, 0x11);
  tx.signature = Bytes(64, 0x22);
  print_hash("txid", tx.txid());

  // Fixed header.
  BlockHeader h;
  h.version = 1;
  h.chain_id = 7;
  h.height = 42;
  h.parent_hash = Hash{};            // all zero
  h.timestamp = 1000;
  h.tx_root.fill(0xAA);
  h.state_root.fill(0xBB);
  h.consensus_data = {};
  print_hash("header_hash", h.hash());

  // State-proof golden: an account state with two entries (addresses 32×0x11
  // and 32×0x22); prove the first against the state root.
  Hash addr_a{}; addr_a.fill(0x11);
  Hash addr_b{}; addr_b.fill(0x22);
  AccountStateMachine st;
  st.credit_genesis(addr_a, 500);
  st.credit_genesis(addr_b, 999);
  print_hash("state_root", st.state_root());
  StateProof pr = st.prove_account(addr_a);
  std::printf("state_proof %s\n", hex(pr.encode()).c_str());

  return 0;
}
