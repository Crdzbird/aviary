#include <string>

#include "aviary/node/demo_node.hpp"
#include "aviary/proofs.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using node::DemoNode;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Hash addr_of(const crypto::KeyPair& kp) { return crypto::address_of(view(kp.public_key)); }
bool contains(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
}  // namespace

int main() {
  init();

  DemoNode node(42);
  auto alice = node.new_account();
  auto bob = node.new_account();
  const Hash alice_addr = addr_of(alice);
  const Hash bob_addr = addr_of(bob);

  std::printf("== creation + genesis ==\n");
  node.credit(alice_addr, 1000);
  CHECK(node.height() == 0);
  CHECK(node.balance(alice_addr) == 1000);

  std::printf("== submit + produce (consensus) ==\n");
  node.submit_transfer(view(alice.secret_key), bob_addr, 100, /*nonce=*/0);
  CHECK(node.mempool_size() == 1);
  CHECK(node.produce_block(1) == 1);
  CHECK(node.balance(alice_addr) == 900);
  CHECK(node.balance(bob_addr) == 100);
  CHECK(node.mempool_size() == 0);

  std::printf("== streaming: events were collected ==\n");
  {
    std::string events = node.drain_events_json();
    CHECK(contains(events, "tx_admitted"));
    CHECK(contains(events, "block_finalized"));
    CHECK(contains(events, "\"height\":1"));
    // Draining clears the buffer.
    CHECK(node.drain_events_json() == "[]");
  }

  std::printf("== observability: metrics ==\n");
  {
    std::string m = node.metrics_text();
    CHECK(contains(m, "aviary_chain_height 1"));
    CHECK(contains(m, "aviary_transactions_total 1"));
  }

  std::printf("== verification: prove alice's balance ==\n");
  {
    StateProof p = node.prove_account(alice_addr);
    CHECK(p.present);
    CHECK(verify_state_proof(node.state_root(), p));
    CHECK(AccountStateMachine::decode_account_leaf(p.leaf_preimage).balance == 900);
  }

  std::printf("== a second block continues the chain ==\n");
  node.submit_transfer(view(alice.secret_key), bob_addr, 50, 1);
  CHECK(node.produce_block(2) == 2);
  CHECK(node.balance(bob_addr) == 150);

  std::printf("== S1 regression: reads do NOT seal genesis ==\n");
  {
    DemoNode fresh(43);
    auto carol = fresh.new_account();
    const Hash carol_addr = addr_of(carol);
    // Read state BEFORE the first credit — under the old semantics this sealed
    // the (empty) genesis and the credit below was silently dropped.
    CHECK(fresh.height() == 0);
    CHECK(fresh.balance(carol_addr) == 0);
    (void)fresh.state_root();
    (void)fresh.metrics_text();
    fresh.credit(carol_addr, 777);  // must still land
    CHECK(fresh.balance(carol_addr) == 777);
    // Pre-start proofs verify against the pre-start root.
    StateProof p0 = fresh.prove_account(carol_addr);
    CHECK(verify_state_proof(fresh.state_root(), p0));
    // ...and the credit survives the first block.
    CHECK(fresh.produce_block(1) == 1);
    CHECK(fresh.balance(carol_addr) == 777);
  }

  std::printf("== S1 regression: credit after sealing throws (never a no-op) ==\n");
  {
    DemoNode sealed_node(44);
    auto dave = sealed_node.new_account();
    const Hash dave_addr = addr_of(dave);
    sealed_node.credit(dave_addr, 10);
    CHECK(sealed_node.produce_block(1) == 1);
    bool threw = false;
    try {
      sealed_node.credit(dave_addr, 1);
    } catch (const node::GenesisSealedError&) {
      threw = true;
    }
    CHECK(threw);
    CHECK(sealed_node.balance(dave_addr) == 10);  // unchanged
  }

  return bctest::summary();
}
