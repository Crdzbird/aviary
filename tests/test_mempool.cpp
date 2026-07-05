#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/mempool.hpp"
#include "blockchainer/utxo_state.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::ed25519_keygen_from_seed(view(seed));
}
Transaction transfer(const crypto::KeyPair& from, const Hash& to,
                     std::uint64_t nonce, codec::u128 amount, std::uint64_t chain) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, from);
  return tx;
}
std::uint64_t nonce_of(const Transaction& tx) {
  return AccountStateMachine::decode_transfer(tx.body).nonce;
}
}  // namespace

int main() {
  init();
  constexpr std::uint64_t kChain = 7;

  auto alice = ed(1);
  auto bob = ed(2);
  auto carol = ed(3);  // never funded, never a recipient → her txs stay invalid
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  AccountStateMachine state;
  state.credit_genesis(alice_addr, 1000);

  std::printf("== admission checks ==\n");
  Mempool mp;
  CHECK(mp.add(transfer(alice, bob_addr, 0, 100, kChain), kChain) == AddResult::kAdded);
  CHECK(mp.add(transfer(alice, bob_addr, 1, 50, kChain), kChain) == AddResult::kAdded);
  CHECK(mp.add(transfer(alice, bob_addr, 2, 25, kChain), kChain) == AddResult::kAdded);
  // Duplicate.
  CHECK(mp.add(transfer(alice, bob_addr, 0, 100, kChain), kChain) == AddResult::kDuplicate);
  // Wrong chain.
  CHECK(mp.add(transfer(alice, bob_addr, 3, 10, 9), kChain) == AddResult::kWrongChain);
  // Bad signature (tamper after signing).
  {
    Transaction bad = transfer(alice, bob_addr, 4, 10, kChain);
    bad.body[0] ^= 0x01;
    CHECK(mp.add(bad, kChain) == AddResult::kInvalidSignature);
  }
  // Unfunded sender is still admitted (may become valid later); selection drops it.
  CHECK(mp.add(transfer(carol, bob_addr, 0, 5, kChain), kChain) == AddResult::kAdded);
  CHECK(mp.size() == 4);

  std::printf("== selection: account nonce ordering, unfunded excluded ==\n");
  {
    auto sel = mp.select(state, 10);
    CHECK(sel.size() == 3);  // alice 0,1,2; carol's unfunded tx excluded
    CHECK(nonce_of(sel[0]) == 0);
    CHECK(nonce_of(sel[1]) == 1);
    CHECK(nonce_of(sel[2]) == 2);
  }

  std::printf("== selection respects a nonce gap ==\n");
  {
    Mempool gap;
    gap.add(transfer(alice, bob_addr, 0, 100, kChain), kChain);
    gap.add(transfer(alice, bob_addr, 2, 25, kChain), kChain);  // skips nonce 1
    auto sel = gap.select(state, 10);
    CHECK(sel.size() == 1);            // only nonce 0 is ready
    CHECK(nonce_of(sel[0]) == 0);
  }

  std::printf("== selection honors max ==\n");
  CHECK(mp.select(state, 2).size() == 2);

  std::printf("== capacity ==\n");
  {
    Mempool small(MempoolConfig{/*max_txs=*/2, /*max_tx_bytes=*/128 * 1024});
    CHECK(small.add(transfer(alice, bob_addr, 0, 1, kChain), kChain) == AddResult::kAdded);
    CHECK(small.add(transfer(alice, bob_addr, 1, 1, kChain), kChain) == AddResult::kAdded);
    CHECK(small.add(transfer(alice, bob_addr, 2, 1, kChain), kChain) == AddResult::kFull);
  }

  std::printf("== per-account cap: one sender cannot flood the pool ==\n");
  {
    MempoolConfig cfg;
    cfg.max_per_account = 2;
    Mempool capped(cfg);
    CHECK(capped.add(transfer(alice, bob_addr, 0, 1, kChain), kChain) == AddResult::kAdded);
    CHECK(capped.add(transfer(alice, bob_addr, 1, 1, kChain), kChain) == AddResult::kAdded);
    // Third alice tx exceeds her cap even though the pool has plenty of room.
    CHECK(capped.add(transfer(alice, bob_addr, 2, 1, kChain), kChain) ==
          AddResult::kAccountLimit);
    // A different sender is unaffected.
    CHECK(capped.add(transfer(bob, alice_addr, 0, 1, kChain), kChain) == AddResult::kAdded);
    // Freeing one of alice's slots lets her back in (count is decremented).
    Transaction a0 = transfer(alice, bob_addr, 0, 1, kChain);
    capped.remove({a0.txid()});
    CHECK(capped.add(transfer(alice, bob_addr, 2, 1, kChain), kChain) == AddResult::kAdded);
  }

  std::printf("== block byte limit bounds selection ==\n");
  {
    const std::size_t one_tx = transfer(alice, bob_addr, 0, 1, kChain).encode().size();
    MempoolConfig lim;
    lim.max_block_bytes = one_tx;  // room for exactly one tx
    Mempool tight(lim);
    tight.add(transfer(alice, bob_addr, 0, 1, kChain), kChain);
    tight.add(transfer(alice, bob_addr, 1, 1, kChain), kChain);
    CHECK(tight.select(state, 100).size() == 1);  // byte budget binds before count
  }

  std::printf("== remove ==\n");
  {
    Transaction t = transfer(alice, bob_addr, 0, 100, kChain);
    std::vector<Hash> ids = {t.txid()};
    std::size_t before = mp.size();
    mp.remove(ids);
    CHECK(mp.size() == before - 1);
    CHECK(!mp.contains(t.txid()));
  }

  std::printf("== UTXO: double-spend conflict excluded from one block ==\n");
  {
    auto a = ed(1);
    const Hash a_addr = crypto::address_of(view(a.public_key));
    const Hash b_addr = crypto::address_of(view(ed(2).public_key));
    UtxoStateMachine ustate;
    auto op0 = ustate.add_genesis_utxo({a_addr, 1000});

    auto spend = [&](codec::u128 amt) {
      Transaction tx;
      tx.version = 1;
      tx.chain_id = kChain;
      tx.body = UtxoStateMachine::encode_body(
          {{op0}, {UtxoStateMachine::Output{b_addr, amt}, UtxoStateMachine::Output{a_addr, 1000 - amt}}});
      sign_transaction(tx, a);
      return tx;
    };
    Mempool ump;
    CHECK(ump.add(spend(100), kChain) == AddResult::kAdded);
    CHECK(ump.add(spend(200), kChain) == AddResult::kAdded);  // conflicts: both spend op0
    auto sel = ump.select(ustate, 10);
    CHECK(sel.size() == 1);  // only one can spend op0
  }

  return bctest::summary();
}
