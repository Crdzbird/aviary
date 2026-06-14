#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/ledger.hpp"
#include "blockchainer/utxo_state.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using Outpoint = UtxoStateMachine::Outpoint;
using Output = UtxoStateMachine::Output;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair key_from(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::ed25519_keygen_from_seed(view(seed));
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

  // Value not conserved: spend 900 change, emit only 800.
  CHECK_THROWS(ledger.build_block(
      {make_spend(alice, {alice_change}, {Output{bob_addr, 800}}, kChain)}, 2));

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

  return bctest::summary();
}
