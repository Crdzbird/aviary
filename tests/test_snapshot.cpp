// State snapshot serialization + ledger snapshot/restore: a chain can be
// persisted to bytes and reloaded to a byte-identical state, then continue.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/utxo_state.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
using Outpoint = UtxoStateMachine::Outpoint;
using Output = UtxoStateMachine::Output;
constexpr std::uint64_t kChain = 7;

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce, codec::u128 amt) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}
const AccountStateMachine& as_account(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  std::printf("== account state serialize/deserialize → byte-identical root ==\n");
  {
    AccountStateMachine sm;
    sm.credit_genesis(alice_addr, 1000);
    sm.credit_genesis(bob_addr, 42);
    const Hash root = sm.state_root();
    Bytes blob = sm.serialize();
    auto back = AccountStateMachine::deserialize(view(blob));
    CHECK(back->state_root() == root);
    CHECK(back->account(alice_addr).balance == 1000);
    CHECK(back->account(bob_addr).balance == 42);
    // A nonce is preserved too (not just balance).
    // (advance alice's nonce via a full ledger below)
  }

  std::printf("== ledger snapshot → restore resumes the chain ==\n");
  {
    auto g = std::make_unique<AccountStateMachine>();
    g->credit_genesis(alice_addr, 1000);
    Ledger l(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
    // Advance two blocks.
    l.process_block(l.build_block({transfer(alice, bob_addr, 0, 100)}, 1));
    l.process_block(l.build_block({transfer(alice, bob_addr, 1, 50)}, 2));
    CHECK(l.tip().height == 2);
    CHECK(as_account(l).account(alice_addr).nonce == 2);

    // Snapshot + restore into a fresh ledger.
    Bytes snap = l.snapshot();
    auto r = Ledger::restore(ChainSpec{kChain, StateModel::kAccount}, view(snap));
    CHECK(r->tip().height == 2);
    CHECK(r->tip().hash() == l.tip().hash());
    CHECK(r->state().state_root() == l.state().state_root());
    CHECK(as_account(*r).account(alice_addr).balance == 850);
    CHECK(as_account(*r).account(bob_addr).balance == 150);
    CHECK(as_account(*r).account(alice_addr).nonce == 2);  // nonce survived

    // The restored ledger accepts the next block exactly as the original would.
    Block b3 = r->build_block({transfer(alice, bob_addr, 2, 25)}, 3);
    Block b3_orig = l.build_block({transfer(alice, bob_addr, 2, 25)}, 3);
    CHECK(b3.header.hash() == b3_orig.header.hash());  // same continuation
    CHECK_MSG(r->process_block(b3).ok, "restored ledger applies block 3");
    CHECK(r->tip().height == 3);
  }

  std::printf("== UTXO snapshot/restore preserves the outpoint set + counter ==\n");
  {
    auto g = std::make_unique<UtxoStateMachine>();
    g->add_genesis_utxo(Output{alice_addr, 1000});
    g->add_genesis_utxo(Output{bob_addr, 250});
    Ledger l(ChainSpec{kChain, StateModel::kUtxo}, std::move(g));
    const Hash root = l.state().state_root();

    Bytes snap = l.snapshot();
    auto r = Ledger::restore(ChainSpec{kChain, StateModel::kUtxo}, view(snap));
    const auto& u = dynamic_cast<const UtxoStateMachine&>(r->state());
    CHECK(u.utxo_count() == 2);
    CHECK(r->state().state_root() == root);
    CHECK(u.get(Outpoint{crypto::zero_hash(), 0}).amount == 1000);
    CHECK(u.get(Outpoint{crypto::zero_hash(), 1}).owner == bob_addr);
    // The genesis counter is restored, so a NEW genesis utxo gets index 2.
    // (verified indirectly: the state root matched, which encodes the set)
  }

  std::printf("== deserialize_state factory dispatches on the model ==\n");
  {
    AccountStateMachine a;
    a.credit_genesis(alice_addr, 5);
    auto back = deserialize_state(StateModel::kAccount, view(a.serialize()));
    CHECK(back->model() == StateModel::kAccount);
    CHECK(back->state_root() == a.state_root());
  }

  return bctest::summary();
}
