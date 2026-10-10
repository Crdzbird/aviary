// Multi-signer UTXO: a transaction spends inputs owned by DIFFERENT parties,
// each authorized by its own per-input witness (kUtxoWitness). Single-author v1
// UTXO and the account model are unaffected.
#include <memory>
#include <vector>

#include "aviary/ledger.hpp"
#include "aviary/utxo_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
using Outpoint = UtxoStateMachine::Outpoint;
using Output = UtxoStateMachine::Output;
using Witness = UtxoStateMachine::Witness;
constexpr std::uint64_t kChain = 7;

// Build a multi-signer UTXO tx: each input is signed by its owner over the tx's
// signing bytes; witnesses (owner order == input order) go in tx.signature.
Transaction multisig_spend(const std::vector<Outpoint>& ins, const std::vector<Output>& outs,
                           const std::vector<crypto::KeyPair>& owners) {
  Transaction tx;
  tx.version = 2;
  tx.chain_id = kChain;
  tx.sig_algo = static_cast<std::uint8_t>(SigAlgo::kUtxoWitness);
  tx.body = UtxoStateMachine::encode_body({ins, outs});
  // author stays empty; each input owner signs signing_bytes().
  const Bytes msg = tx.signing_bytes();
  std::vector<Witness> ws;
  for (const auto& kp : owners) {
    Witness w;
    w.pubkey = kp.public_key;
    w.signature = crypto::ed25519_sign(view(msg), view(kp.secret_key));
    ws.push_back(std::move(w));
  }
  tx.signature = UtxoStateMachine::encode_witnesses(ws);
  return tx;
}

const UtxoStateMachine& as_utxo(const Ledger& l) {
  return dynamic_cast<const UtxoStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  auto alice = ed(1), bob = ed(2), carol = ed(3);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));
  const Hash carol_addr = crypto::address_of(view(carol.public_key));

  // Genesis: alice and bob each own an output.
  auto g = std::make_unique<UtxoStateMachine>();
  Outpoint a_utxo = g->add_genesis_utxo(Output{alice_addr, 600});
  Outpoint b_utxo = g->add_genesis_utxo(Output{bob_addr, 400});
  Ledger l(ChainSpec{kChain, StateModel::kUtxo}, std::move(g));

  std::printf("== a tx spends inputs from two different owners ==\n");
  {
    // alice + bob jointly fund a 1000 output to carol; each signs their input.
    Transaction tx =
        multisig_spend({a_utxo, b_utxo}, {Output{carol_addr, 1000}}, {alice, bob});
    Block blk = l.build_block({tx}, 1);
    CHECK_MSG(l.process_block(blk).ok, "multi-owner spend accepted");
    CHECK(l.tip().height == 1);
    CHECK(!as_utxo(l).has(a_utxo) && !as_utxo(l).has(b_utxo));  // both spent
    CHECK(as_utxo(l).get(Outpoint{tx.txid(), 0}).owner == carol_addr);
    CHECK(as_utxo(l).get(Outpoint{tx.txid(), 0}).amount == 1000);
  }

  std::printf("== adversarial: a wrong signer for an input is rejected ==\n");
  {
    auto g2 = std::make_unique<UtxoStateMachine>();
    Outpoint a2 = g2->add_genesis_utxo(Output{alice_addr, 600});
    Outpoint b2 = g2->add_genesis_utxo(Output{bob_addr, 400});
    Ledger l2(ChainSpec{kChain, StateModel::kUtxo}, std::move(g2));

    // Bob signs alice's input (carol signs bob's) → both witnesses mis-own.
    Transaction bad =
        multisig_spend({a2, b2}, {Output{carol_addr, 1000}}, {bob, carol});
    CHECK_THROWS(l2.build_block({bad}, 1));

    // A tampered witness signature is rejected.
    Transaction ok = multisig_spend({a2, b2}, {Output{carol_addr, 1000}}, {alice, bob});
    Transaction tampered = ok;
    tampered.signature[tampered.signature.size() / 2] ^= 0x01;
    CHECK_THROWS(l2.build_block({tampered}, 1));

    // Wrong witness count (one witness for two inputs) is rejected.
    Transaction short_wit = multisig_spend({a2, b2}, {Output{carol_addr, 1000}}, {alice});
    CHECK_THROWS(l2.build_block({short_wit}, 1));

    // The correct multi-sig still applies (chain untouched by the failures).
    CHECK(l2.tip().height == 0);
    Block good = l2.build_block({ok}, 1);
    CHECK(l2.process_block(good).ok);
  }

  std::printf("== a multi-signer tx can also pay a fee (inputs > outputs) ==\n");
  {
    auto g3 = std::make_unique<UtxoStateMachine>();
    Outpoint a3 = g3->add_genesis_utxo(Output{alice_addr, 600});
    Outpoint b3 = g3->add_genesis_utxo(Output{bob_addr, 400});
    Ledger l3(ChainSpec{kChain, StateModel::kUtxo}, std::move(g3));
    // 1000 in, 950 out → 50 burned as fee.
    Transaction tx = multisig_spend({a3, b3}, {Output{carol_addr, 950}}, {alice, bob});
    CHECK(as_utxo(l3).flat_fee() == 0);
    Block blk = l3.build_block({tx}, 1);
    CHECK(l3.process_block(blk).ok);
    CHECK(as_utxo(l3).get(Outpoint{tx.txid(), 0}).amount == 950);
  }

  return bctest::summary();
}
