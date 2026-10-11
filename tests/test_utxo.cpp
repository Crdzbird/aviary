#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <vector>

#include "aviary/ledger.hpp"
#include "aviary/merkle.hpp"
#include "aviary/utxo_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using Outpoint = UtxoStateMachine::Outpoint;
using Output = UtxoStateMachine::Output;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair key_from(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::ed25519_keygen_from_seed(view(seed));
}

// Naive reference root: the pre-COW definition - compute_root over the sorted
// UTXO leaves. The copy-on-write fast path must equal this for every state.
Hash naive_utxo_root(const std::map<Outpoint, Output>& utxos) {
  std::vector<Hash> leaves;
  leaves.reserve(utxos.size());
  for (const auto& [op, out] : utxos) {
    codec::CodecWriter w;
    w.write_fixed_bytes(codec::ByteView(op.txid.data(), 32), 32);
    w.write_u32(op.index);
    w.write_fixed_bytes(codec::ByteView(out.owner.data(), 32), 32);
    w.write_u128(out.amount);
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, w.take()));
  }
  return merkle::compute_root(leaves);
}

Transaction make_spend(const crypto::KeyPair& signer,
                       const std::vector<Outpoint>& inputs,
                       const std::vector<Output>& outputs,
                       std::uint64_t chain_id) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain_id;
  tx.body = UtxoStateMachine::encode_body({inputs, outputs});
  sign_transaction(tx, signer);
  return tx;
}

const UtxoStateMachine& as_utxo(const Ledger& l) {
  return dynamic_cast<const UtxoStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  constexpr std::uint64_t kChain = 7;

  auto alice = key_from(1);
  auto bob = key_from(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  auto genesis = std::make_unique<UtxoStateMachine>();
  const Outpoint op0 = genesis->add_genesis_utxo(Output{alice_addr, 1000});
  Ledger ledger(ChainSpec{kChain, StateModel::kUtxo}, std::move(genesis));

  std::printf("== pluggability: same Ledger, UTXO state ==\n");
  CHECK(ledger.state().model() == StateModel::kUtxo);
  CHECK(as_utxo(ledger).utxo_count() == 1);
  CHECK(as_utxo(ledger).has(op0));

  std::printf("== spend genesis UTXO into two outputs ==\n");
  Transaction t0 = make_spend(alice, {op0},
                              {Output{bob_addr, 100}, Output{alice_addr, 900}}, kChain);
  Block b1 = ledger.build_block({t0}, 1);
  BlockError e1 = ledger.process_block(b1);
  CHECK_MSG(e1.ok, e1.ok ? "block 1 accepted" : e1.error.c_str());
  CHECK(ledger.tip().height == 1);

  const Hash txid0 = t0.txid();
  const Outpoint to_bob{txid0, 0};
  const Outpoint alice_change{txid0, 1};
  CHECK(!as_utxo(ledger).has(op0));                 // genesis output spent
  CHECK(as_utxo(ledger).utxo_count() == 2);
  CHECK(as_utxo(ledger).has(to_bob));
  CHECK(as_utxo(ledger).has(alice_change));
  CHECK(as_utxo(ledger).get(to_bob).amount == 100);
  CHECK(as_utxo(ledger).get(to_bob).owner == bob_addr);
  CHECK(as_utxo(ledger).get(alice_change).amount == 900);

  std::printf("== determinism: same scenario rebuilds the same block ==\n");
  {
    auto g2 = std::make_unique<UtxoStateMachine>();
    const Outpoint g2op = g2->add_genesis_utxo(Output{alice_addr, 1000});
    Ledger l2(ChainSpec{kChain, StateModel::kUtxo}, std::move(g2));
    Transaction t0b = make_spend(alice, {g2op},
                                 {Output{bob_addr, 100}, Output{alice_addr, 900}}, kChain);
    Block b1b = l2.build_block({t0b}, 1);
    CHECK(b1b.header.hash() == b1.header.hash());
  }

  std::printf("== adversarial ==\n");
  // Double-spend the already-spent genesis output.
  CHECK_THROWS(ledger.build_block(
      {make_spend(alice, {op0}, {Output{alice_addr, 1000}}, kChain)}, 2));

  // Alice tries to spend Bob's output (not owned by signer).
  CHECK_THROWS(ledger.build_block(
      {make_spend(alice, {to_bob}, {Output{alice_addr, 100}}, kChain)}, 2));

  // Minting: outputs exceed inputs (spend 900 change, emit 1000). Rejected - 
  // a spend may burn value as a fee (in > out) but can never create it (out > in).
  CHECK_THROWS(ledger.build_block(
      {make_spend(alice, {alice_change}, {Output{bob_addr, 1000}}, kChain)}, 2));

  // But spending 900 and emitting only 800 is now VALID - the 100 surplus is a
  // burned fee (a feeless chain requires only out <= in). build_block accepts it
  // (throws only on an invalid tx); we don't commit it, to leave the chain at
  // height 1 for the cases below.
  {
    Block feeblk = ledger.build_block(
        {make_spend(alice, {alice_change}, {Output{bob_addr, 800}}, kChain)}, 2);
    CHECK(feeblk.transactions.size() == 1);  // built => the in>out fee spend is valid
  }

  // Tampered state_root rejected; ledger unchanged.
  {
    Transaction t = make_spend(alice, {alice_change},
                               {Output{bob_addr, 900}}, kChain);
    Block good = ledger.build_block({t}, 2);
    Block tampered = good;
    tampered.header.state_root[0] ^= 0x01;
    BlockError er = ledger.process_block(tampered);
    CHECK(!er.ok);
    CHECK(ledger.tip().height == 1);
    CHECK(as_utxo(ledger).utxo_count() == 2);  // untouched
  }

  std::printf("== happy path: Bob spends his output ==\n");
  {
    Transaction t = make_spend(bob, {to_bob}, {Output{alice_addr, 100}}, kChain);
    Block b2 = ledger.build_block({t}, 2);
    BlockError er = ledger.process_block(b2);
    CHECK_MSG(er.ok, er.ok ? "block 2 accepted" : er.error.c_str());
    CHECK(ledger.tip().height == 2);
    CHECK(!as_utxo(ledger).has(to_bob));
  }

  std::printf("== fee floor: a spend must clear the chain minimum ==\n");
  {
    UtxoStateMachine s(/*flat_fee=*/10);
    Outpoint op = s.add_genesis_utxo(Output{alice_addr, 1000});
    // in 1000, out 995 -> fee 5 < 10 -> rejected.
    CHECK(!s.validate(make_spend(alice, {op}, {Output{bob_addr, 995}}, kChain)).ok);
    // in 1000, out 990 -> fee 10 == minimum -> accepted.
    CHECK(s.validate(make_spend(alice, {op}, {Output{bob_addr, 990}}, kChain)).ok);
    // Exact conservation (fee 0) is below the minimum on a fee'd chain -> rejected.
    CHECK(!s.validate(make_spend(alice, {op}, {Output{bob_addr, 1000}}, kChain)).ok);
    // Minting (out > in) is always rejected.
    CHECK(!s.validate(make_spend(alice, {op}, {Output{bob_addr, 1001}}, kChain)).ok);
  }

  std::printf("== copy-on-write differential: fast root tracks the naive root ==\n");
  {
    auto owner = key_from(9);
    const Hash owner_addr = crypto::address_of(view(owner.public_key));
    UtxoStateMachine sm;  // feeless
    std::map<Outpoint, Output> ref;
    std::vector<Outpoint> live;
    std::mt19937_64 rng(0xC0FFEEULL);

    auto add = [&](codec::u128 amt) {
      Outpoint op = sm.add_genesis_utxo(Output{owner_addr, amt});
      ref[op] = Output{owner_addr, amt};
      live.push_back(op);
    };
    for (int i = 0; i < 20; ++i) add(1000 + i);
    CHECK(sm.state_root() == naive_utxo_root(ref));

    for (int step = 0; step < 600; ++step) {
      if (live.empty() || rng() % 3 == 0) {
        add(500 + rng() % 500);
      } else {
        // Spend 1..3 distinct live outpoints into 1..2 outputs, burning a fee.
        const int k = 1 + static_cast<int>(rng() % std::min<std::size_t>(3, live.size()));
        std::vector<Outpoint> ins;
        codec::u128 in_sum = 0;
        for (int j = 0; j < k && !live.empty(); ++j) {
          const std::size_t idx = rng() % live.size();
          ins.push_back(live[idx]);
          in_sum += ref[live[idx]].amount;
          live.erase(live.begin() + idx);
        }
        codec::u128 fee = in_sum == 0 ? 0 : rng() % std::min<codec::u128>(50, in_sum);
        const codec::u128 out_total = in_sum - fee;
        const codec::u128 first = out_total / 2;
        std::vector<Output> outs = {Output{owner_addr, first},
                                    Output{owner_addr, out_total - first}};
        Transaction tx = make_spend(owner, ins, outs, kChain);
        CHECK(sm.validate(tx).ok);
        sm.apply(tx);
        for (const auto& op : ins) ref.erase(op);
        const Hash txid = tx.txid();
        for (std::uint32_t i = 0; i < outs.size(); ++i) {
          Outpoint np{txid, i};
          ref[np] = outs[i];
          live.push_back(np);
        }
      }
      if (step < 120 || step % 5 == 0) {
        CHECK_MSG(sm.state_root() == naive_utxo_root(ref), "root matches naive after op");
      }
    }
    CHECK_MSG(sm.state_root() == naive_utxo_root(ref), "final root matches naive");
    CHECK(sm.utxo_count() == ref.size());
    if (!live.empty()) CHECK(sm.prove_outpoint(live.front()).present);
  }

  std::printf("== clone is copy-on-write independent ==\n");
  {
    auto owner = key_from(8);
    const Hash owner_addr = crypto::address_of(view(owner.public_key));
    UtxoStateMachine a;
    std::vector<Outpoint> ops;
    for (int i = 0; i < 50; ++i) ops.push_back(a.add_genesis_utxo(Output{owner_addr, 100}));
    const Hash root_a = a.state_root();

    auto b = a.clone();
    auto* bp = dynamic_cast<UtxoStateMachine*>(b.get());
    bp->apply(make_spend(owner, {ops[0]}, {Output{owner_addr, 100}}, kChain));  // mutate clone

    CHECK_MSG(a.state_root() == root_a, "original root unchanged after clone mutated");
    CHECK(a.has(ops[0]));       // original still holds the spent outpoint
    CHECK(!bp->has(ops[0]));    // clone spent it
  }

  return bctest::summary();
}
