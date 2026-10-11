#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/credential_gate.hpp"
#include "aviary/mempool.hpp"
#include "aviary/utxo_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

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

// A version-2 transfer with a declared priority-fee tip.
Transaction transfer_tip(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce,
                         codec::u128 amount, codec::u128 tip, std::uint64_t chain) {
  Transaction tx;
  tx.version = 2;
  tx.chain_id = chain;
  tx.sig_algo = static_cast<std::uint8_t>(SigAlgo::kEd25519);
  tx.fee = tip;
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
  auto carol = ed(3);  // never funded, never a recipient -> her txs stay invalid
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

  std::printf("== fee-priority eviction: a full pool keeps the highest bidders ==\n");
  {
    auto s1 = ed(10), s2 = ed(11), s3 = ed(12), s4 = ed(13);
    MempoolConfig cfg;
    cfg.max_txs = 2;
    Mempool fee(cfg);
    Transaction t5 = transfer_tip(s1, bob_addr, 0, 1, /*tip=*/5, kChain);
    Transaction t7 = transfer_tip(s2, bob_addr, 0, 1, /*tip=*/7, kChain);
    CHECK(fee.add(t5, kChain) == AddResult::kAdded);
    CHECK(fee.add(t7, kChain) == AddResult::kAdded);  // pool full (fees 5,7)
    // A higher bid (10) evicts the cheapest resident (5).
    Transaction t10 = transfer_tip(s3, bob_addr, 0, 1, /*tip=*/10, kChain);
    CHECK(fee.add(t10, kChain) == AddResult::kAdded);
    CHECK(!fee.contains(t5.txid()));  // fee-5 evicted
    CHECK(fee.contains(t7.txid()) && fee.contains(t10.txid()));
    // A low bid (1) cannot displace the cheapest remaining resident (7).
    Transaction t1 = transfer_tip(s4, bob_addr, 0, 1, /*tip=*/1, kChain);
    CHECK(fee.add(t1, kChain) == AddResult::kFull);
  }

  std::printf("== selection is fee-priority ordered ==\n");
  {
    auto s1 = ed(20), s2 = ed(21), s3 = ed(22);
    AccountStateMachine st;
    st.credit_genesis(crypto::address_of(view(s1.public_key)), 1000);
    st.credit_genesis(crypto::address_of(view(s2.public_key)), 1000);
    st.credit_genesis(crypto::address_of(view(s3.public_key)), 1000);
    Mempool mp2;
    mp2.add(transfer_tip(s1, bob_addr, 0, 1, /*tip=*/5, kChain), kChain);
    mp2.add(transfer_tip(s2, bob_addr, 0, 1, /*tip=*/10, kChain), kChain);
    mp2.add(transfer_tip(s3, bob_addr, 0, 1, /*tip=*/1, kChain), kChain);
    auto sel = mp2.select(st, 10);
    CHECK(sel.size() == 3);
    CHECK(sel[0].declared_fee() == 10);  // highest tip first
    CHECK(sel[1].declared_fee() == 5);
    CHECK(sel[2].declared_fee() == 1);
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

  // -- Membership admission gate (CredentialGate, Option A) ---------------------
  // A permissioned pool admits a tx only behind a valid "submit"-scope
  // presentation. The plain add() is the trusted path and stays ungated.
  {
    auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x5A)));
    consensus::CredentialPolicy policy;
    policy.chain_id = kChain;
    policy.issuer_public_key = issuer.public_key;
    policy.header = Bytes{'m', 'e', 'm', 'b', 'e', 'r'};
    policy.min_tier = 1;
    consensus::CredentialGate gate(policy);

    Bytes member_secret(32, 0x11);
    auto cred = consensus::issue_credential(policy, view(issuer.secret_key), view(member_secret),
                                            /*tier=*/2, /*not_after=*/4102444800ULL,
                                            /*issuer_epoch=*/1);
    const std::uint64_t now = 1700000000ULL;
    Bytes challenge(32, 0x42);

    Mempool gated;
    gated.set_credential_gate(&gate);
    auto tx = transfer(alice, bob_addr, 0, 10, kChain);

    // Without a valid presentation, the gated path refuses the tx.
    CHECK(gated.add(tx, kChain, 3, view(challenge), view(Bytes{0, 1, 2}), now) ==
          AddResult::kNotAdmitted);
    CHECK(gated.size() == 0);

    // A valid "submit" presentation for (epoch 3, this challenge) admits it.
    Bytes pres = consensus::CredentialGate::present(policy, cred, 3, "submit", view(challenge));
    CHECK(gated.add(tx, kChain, 3, view(challenge), view(pres), now) == AddResult::kAdded);
    CHECK(gated.size() == 1);

    // A presentation bound to a different epoch does not admit a fresh tx.
    auto tx2 = transfer(alice, bob_addr, 1, 10, kChain);
    CHECK(gated.add(tx2, kChain, 4, view(challenge), view(pres), now) == AddResult::kNotAdmitted);

    // The plain add() is the trusted path - never gated, even with a gate set.
    CHECK(gated.add(tx2, kChain) == AddResult::kAdded);

    // An ungated pool ignores the presentation argument entirely (permissionless).
    Mempool open;
    CHECK(open.add(transfer(alice, bob_addr, 0, 10, kChain), kChain, 0, view(challenge),
                   view(Bytes{}), now) == AddResult::kAdded);
  }

  return bctest::summary();
}
