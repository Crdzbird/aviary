#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair key_from(std::uint8_t seed_byte) {
  Bytes seed(32, seed_byte);
  return crypto::ed25519_keygen_from_seed(view(seed));
}

Transaction make_transfer(const crypto::KeyPair& from, const Hash& to,
                          std::uint64_t nonce, codec::u128 amount,
                          std::uint64_t chain_id) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain_id;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, from);
  return tx;
}

const AccountStateMachine& as_account(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  constexpr std::uint64_t kChain = 7;

  auto alice = key_from(1);
  auto bob = key_from(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  auto genesis = std::make_unique<AccountStateMachine>();
  genesis->credit_genesis(alice_addr, 1000);
  Ledger ledger(ChainSpec{kChain, StateModel::kAccount}, std::move(genesis));

  CHECK(ledger.tip().height == 0);
  CHECK(as_account(ledger).account(alice_addr).balance == 1000);

  std::printf("== produce + apply a block ==\n");
  Transaction t0 = make_transfer(alice, bob_addr, /*nonce=*/0, /*amount=*/100, kChain);
  Block b1 = ledger.build_block({t0}, /*timestamp=*/1);
  BlockError e1 = ledger.process_block(b1);
  CHECK_MSG(e1.ok, e1.ok ? "block 1 accepted" : e1.error.c_str());
  CHECK(ledger.tip().height == 1);
  CHECK(ledger.tip().hash() == b1.header.hash());
  CHECK(as_account(ledger).account(alice_addr).balance == 900);
  CHECK(as_account(ledger).account(alice_addr).nonce == 1);
  CHECK(as_account(ledger).account(bob_addr).balance == 100);

  std::printf("== determinism: rebuilding the same block gives the same hash ==\n");
  {
    // A fresh ledger in the same genesis state must reproduce b1 byte-for-byte.
    auto g2 = std::make_unique<AccountStateMachine>();
    g2->credit_genesis(alice_addr, 1000);
    Ledger l2(ChainSpec{kChain, StateModel::kAccount}, std::move(g2));
    Transaction t0b = make_transfer(alice, bob_addr, 0, 100, kChain);
    Block b1b = l2.build_block({t0b}, 1);
    CHECK(b1b.header.hash() == b1.header.hash());
    CHECK(b1b.encode() == b1.encode());
  }

  std::printf("== adversarial: tampered block rejected, ledger unchanged ==\n");
  {
    Transaction t1 = make_transfer(alice, bob_addr, 1, 50, kChain);
    Block good = ledger.build_block({t1}, 2);
    Block tampered = good;
    tampered.header.state_root[0] ^= 0x01;  // lie about the resulting state
    const Hash root_before = ledger.tip().state_root;
    BlockError er = ledger.process_block(tampered);
    CHECK(!er.ok);
    CHECK(ledger.tip().height == 1);                 // not advanced
    CHECK(ledger.tip().state_root == root_before);   // state untouched
    CHECK(as_account(ledger).account(bob_addr).balance == 100);
  }

  std::printf("== adversarial: nonce replay rejected ==\n");
  {
    // t0 used nonce 0, already consumed. Re-submitting must fail to build.
    CHECK_THROWS(ledger.build_block({make_transfer(alice, bob_addr, 0, 10, kChain)}, 2));
  }

  std::printf("== adversarial: overdraft rejected ==\n");
  {
    CHECK_THROWS(ledger.build_block({make_transfer(alice, bob_addr, 1, 999999, kChain)}, 2));
  }

  std::printf("== adversarial: wrong chain_id rejected ==\n");
  {
    CHECK_THROWS(ledger.build_block({make_transfer(alice, bob_addr, 1, 10, /*chain=*/9)}, 2));
  }

  std::printf("== adversarial: wrong parent rejected ==\n");
  {
    Transaction t1 = make_transfer(alice, bob_addr, 1, 10, kChain);
    Block b = ledger.build_block({t1}, 2);
    b.header.parent_hash[0] ^= 0x01;
    BlockError er = ledger.process_block(b);
    CHECK(!er.ok);
  }

  std::printf("== adversarial: atomicity — one bad tx voids the whole block ==\n");
  {
    Transaction good = make_transfer(alice, bob_addr, 1, 10, kChain);
    Transaction bad = make_transfer(alice, bob_addr, 1, 10, kChain);  // dup nonce 1
    // Build manually so we can include both (build_block would reject the dup).
    Block b;
    b.header.version = 1;
    b.header.chain_id = kChain;
    b.header.height = 2;
    b.header.parent_hash = ledger.tip().hash();
    b.header.timestamp = 2;
    b.transactions = {good, bad};
    b.header.tx_root = compute_tx_root(b.transactions);
    b.header.state_root = ledger.tip().state_root;  // arbitrary; should never commit
    BlockError er = ledger.process_block(b);
    CHECK(!er.ok);
    CHECK(ledger.tip().height == 1);  // nothing applied
    CHECK(as_account(ledger).account(alice_addr).balance == 900);
  }

  std::printf("== happy path continues: second block applies ==\n");
  {
    Transaction t1 = make_transfer(alice, bob_addr, 1, 50, kChain);
    Block b2 = ledger.build_block({t1}, 3);
    BlockError er = ledger.process_block(b2);
    CHECK_MSG(er.ok, er.ok ? "block 2 accepted" : er.error.c_str());
    CHECK(ledger.tip().height == 2);
    CHECK(as_account(ledger).account(alice_addr).balance == 850);
    CHECK(as_account(ledger).account(bob_addr).balance == 150);
  }

  std::printf("== flat protocol fee: burned from sender, chain param ==\n");
  {
    constexpr codec::u128 kFee = 10;
    // The genesis state machine's fee must match the chain spec's flat_fee.
    auto g = std::make_unique<AccountStateMachine>(kFee);
    g->credit_genesis(alice_addr, 1000);
    ChainSpec spec{kChain, StateModel::kAccount};
    spec.flat_fee = kFee;
    Ledger fl(spec, std::move(g));

    Transaction t = make_transfer(alice, bob_addr, 0, 100, kChain);
    Block fb = fl.build_block({t}, 1);
    CHECK_MSG(fl.process_block(fb).ok, "fee block accepted");
    // Sender pays amount + fee; recipient gets amount; fee is burned.
    CHECK(as_account(fl).account(alice_addr).balance == 1000 - 100 - kFee);  // 890
    CHECK(as_account(fl).account(bob_addr).balance == 100);

    // Determinism: same inputs → same block hash (fee folded into the root).
    auto g2 = std::make_unique<AccountStateMachine>(kFee);
    g2->credit_genesis(alice_addr, 1000);
    Ledger fl2(spec, std::move(g2));
    Block fb2 = fl2.build_block({make_transfer(alice, bob_addr, 0, 100, kChain)}, 1);
    CHECK(fb2.header.hash() == fb.header.hash());

    // A non-zero fee changes the committed state vs a feeless chain.
    auto gz = std::make_unique<AccountStateMachine>(0);
    gz->credit_genesis(alice_addr, 1000);
    Ledger flz(ChainSpec{kChain, StateModel::kAccount}, std::move(gz));
    Block fbz = flz.build_block({make_transfer(alice, bob_addr, 0, 100, kChain)}, 1);
    CHECK(fbz.header.state_root != fb.header.state_root);

    // Sender must cover amount + fee: 995 + fee(10) > 1000 → rejected.
    CHECK_THROWS(fl.build_block({make_transfer(alice, bob_addr, 1, 995, kChain)}, 2));
    // Exactly 890 balance can send at most 880 (+10 fee).
    Block ok = fl.build_block({make_transfer(alice, bob_addr, 1, 880, kChain)}, 2);
    CHECK_MSG(fl.process_block(ok).ok, "spend-to-the-fee-limit accepted");
    CHECK(as_account(fl).account(alice_addr).balance == 0);
  }

  return bctest::summary();
}
