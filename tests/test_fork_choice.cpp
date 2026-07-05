// Fork choice + reorg: a tree of competing blocks, canonical head on the
// heaviest branch, and an observable state reorganization when a side branch
// overtakes.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/fork_choice.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 7;

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce, codec::u128 amt) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}
const AccountStateMachine& acc(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  auto alice = ed(1);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(ed(2).public_key));
  const Hash carol_addr = crypto::address_of(view(ed(3).public_key));

  // Deterministic genesis factory: alice funded with 1000.
  auto genesis = [&]() -> std::unique_ptr<StateMachine> {
    auto g = std::make_unique<AccountStateMachine>();
    g->credit_genesis(alice_addr, 1000);
    return g;
  };
  ForkChoice fc(ChainSpec{kChain, StateModel::kAccount}, genesis);

  // Producers for two competing branches, each a real ledger building valid
  // blocks. Branch A sends alice → bob; branch B sends alice → carol.
  auto producerA = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, genesis());
  auto producerB = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, genesis());

  Block a1 = producerA->build_block({transfer(alice, bob_addr, 0, 100)}, 1);
  producerA->process_block(a1);
  Block a2 = producerA->build_block({transfer(alice, bob_addr, 1, 50)}, 2);
  producerA->process_block(a2);

  Block b1 = producerB->build_block({transfer(alice, carol_addr, 0, 200)}, 10);
  producerB->process_block(b1);
  Block b2 = producerB->build_block({transfer(alice, carol_addr, 1, 100)}, 11);
  producerB->process_block(b2);
  Block b3 = producerB->build_block({transfer(alice, carol_addr, 2, 50)}, 12);
  producerB->process_block(b3);

  std::printf("== linear extension advances the head (no reorg) ==\n");
  {
    auto r1 = fc.add_block(a1);
    CHECK(r1.accepted && !r1.reorg);
    auto r2 = fc.add_block(a2);
    CHECK(r2.accepted && !r2.reorg);
    CHECK(fc.head() == a2.header.hash());
    CHECK(fc.head_height() == 2);
    // Canonical state is branch A.
    CHECK(acc(fc.canonical()).account(bob_addr).balance == 150);
    CHECK(acc(fc.canonical()).account(carol_addr).balance == 0);
    CHECK(acc(fc.canonical()).account(alice_addr).balance == 850);
  }

  std::printf("== a shorter/equal side branch is stored but not canonical ==\n");
  {
    auto rb1 = fc.add_block(b1);  // cumulative 1 < 2 → not head
    CHECK(rb1.accepted && !rb1.reorg);
    CHECK(fc.head() == a2.header.hash());  // still branch A
    auto rb2 = fc.add_block(b2);  // cumulative 2 == 2 → tie keeps incumbent
    CHECK(rb2.accepted && !rb2.reorg);
    CHECK(fc.head() == a2.header.hash());
    CHECK(fc.knows(b1.header.hash()) && fc.knows(b2.header.hash()));
    // Canonical state is unchanged (still branch A).
    CHECK(acc(fc.canonical()).account(bob_addr).balance == 150);
  }

  std::printf("== a heavier branch triggers a reorg; canonical state changes ==\n");
  {
    auto rb3 = fc.add_block(b3);  // cumulative 3 > 2 → REORG to branch B
    CHECK(rb3.accepted && rb3.reorg);
    CHECK(fc.head() == b3.header.hash());
    CHECK(fc.head_height() == 3);
    // The state now reflects branch B — bob's credit is gone, carol has hers.
    CHECK(acc(fc.canonical()).account(carol_addr).balance == 350);
    CHECK(acc(fc.canonical()).account(bob_addr).balance == 0);
    CHECK(acc(fc.canonical()).account(alice_addr).balance == 650);
  }

  std::printf("== extending the new head is a fast, non-reorg advance ==\n");
  {
    Block b4 = producerB->build_block({transfer(alice, carol_addr, 3, 10)}, 13);
    producerB->process_block(b4);
    auto r = fc.add_block(b4);
    CHECK(r.accepted && !r.reorg);
    CHECK(fc.head() == b4.header.hash());
    CHECK(fc.head_height() == 4);
    CHECK(acc(fc.canonical()).account(carol_addr).balance == 360);
  }

  std::printf("== rejects: duplicate, unknown parent, invalid state transition ==\n");
  {
    const Hash head_before = fc.head();  // current tip (b4)
    CHECK(!fc.add_block(b3).accepted);  // duplicate
    // Unknown parent: a block whose parent is not in the tree.
    Block orphan = a1;
    orphan.header.parent_hash[0] ^= 0x77;
    CHECK(!fc.add_block(orphan).accepted);
    // Invalid state transition: a block spending an already-used nonce on the
    // canonical branch. Build one on a fresh producer at the wrong state.
    auto bad_producer = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, genesis());
    Block valid_first = bad_producer->build_block({transfer(alice, bob_addr, 0, 10)}, 20);
    // Tamper the state_root so process_block rejects it during replay/validation.
    Block tampered = valid_first;
    tampered.header.state_root[0] ^= 0x01;
    // Recompute the header hash by re-encoding is implicit; parent is genesis.
    CHECK(!fc.add_block(tampered).accepted);
    // The head is unchanged by every rejection.
    CHECK(fc.head() == head_before);
  }

  return bctest::summary();
}
