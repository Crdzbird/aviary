#include <string>

#include "blockchainer/node/demo_node.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
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
    CHECK(contains(m, "blockchainer_chain_height 1"));
    CHECK(contains(m, "blockchainer_transactions_total 1"));
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

  return bctest::summary();
}
