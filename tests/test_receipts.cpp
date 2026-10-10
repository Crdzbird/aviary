// Receipts + receipt root: version-2 blocks commit one execution receipt per tx.
// v1 blocks (default) are unaffected. Covers the header gate, fee accounting for
// both models, receipt-inclusion proofs, and receipt_root validation.
#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/ledger.hpp"
#include "aviary/proofs.hpp"
#include "aviary/utxo_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
using Outpoint = UtxoStateMachine::Outpoint;
using Output = UtxoStateMachine::Output;

constexpr std::uint64_t kChain = 7;

Transaction transfer_tip(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce,
                         codec::u128 amount, codec::u128 tip) {
  Transaction tx;
  tx.version = 2;
  tx.chain_id = kChain;
  tx.sig_algo = static_cast<std::uint8_t>(SigAlgo::kEd25519);
  tx.fee = tip;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, from);
  return tx;
}

Transaction utxo_spend(const crypto::KeyPair& signer, const std::vector<Outpoint>& ins,
                       const std::vector<Output>& outs) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = UtxoStateMachine::encode_body({ins, outs});
  sign_transaction(tx, signer);
  return tx;
}

ChainSpec receipt_spec(codec::u128 fee, StateModel model = StateModel::kAccount) {
  ChainSpec s{kChain, model};
  s.flat_fee = fee;
  s.produce_receipts = true;
  return s;
}
}  // namespace

int main() {
  init();
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  std::printf("== account receipts: version-2 block commits a receipt per tx ==\n");
  {
    auto g = std::make_unique<AccountStateMachine>(10);
    g->credit_genesis(alice_addr, 1000);
    Ledger l(receipt_spec(10), std::move(g));

    // Two transfers: fees are flat(10) + tip.
    Transaction t0 = transfer_tip(alice, bob_addr, 0, 100, 5);   // fee 15
    Transaction t1 = transfer_tip(alice, bob_addr, 1, 50, 0);    // fee 10
    Block b = l.build_block({t0, t1}, 1);
    CHECK(b.header.version == 2);
    CHECK_MSG(l.process_block(b).ok, "v2 block accepted");

    const auto& receipts = l.last_receipts();
    CHECK(receipts.size() == 2);
    CHECK(receipts[0].status == 1 && receipts[0].fee == 15);
    CHECK(receipts[1].status == 1 && receipts[1].fee == 10);
    CHECK(b.header.receipt_root == compute_receipt_root(receipts));
    CHECK(b.header.receipt_root != Hash{});  // non-trivial

    std::printf("== receipt-inclusion proof verifies; tamper is rejected ==\n");
    ReceiptProof p = receipt_inclusion_proof(receipts, 0);
    CHECK(verify_receipt_inclusion(b.header.receipt_root, p));
    // A proof for the other receipt does not verify at index 0's leaf... verify
    // the second one properly, and a wrong root fails.
    ReceiptProof p1 = receipt_inclusion_proof(receipts, 1);
    CHECK(verify_receipt_inclusion(b.header.receipt_root, p1));
    CHECK(!verify_receipt_inclusion(Hash{}, p));  // wrong root
    ReceiptProof bad = p;
    bad.receipt_encoding[1] ^= 0x01;  // corrupt the receipt's fee byte
    CHECK(!verify_receipt_inclusion(b.header.receipt_root, bad));

    std::printf("== header v2 encode/decode round-trips the receipt root ==\n");
    BlockHeader back = BlockHeader::decode_all(view(b.header.encode()));
    CHECK(back.version == 2);
    CHECK(back.receipt_root == b.header.receipt_root);
    CHECK(back.hash() == b.header.hash());
  }

  std::printf("== a tampered receipt_root is rejected by process_block ==\n");
  {
    auto g = std::make_unique<AccountStateMachine>(10);
    g->credit_genesis(alice_addr, 1000);
    Ledger l(receipt_spec(10), std::move(g));
    Block good = l.build_block({transfer_tip(alice, bob_addr, 0, 100, 5)}, 1);
    Block tampered = good;
    tampered.header.receipt_root[0] ^= 0x01;
    CHECK(!l.process_block(tampered).ok);
    CHECK(l.tip().height == 0);  // unchanged
  }

  std::printf("== feeless / version-1 chains carry no receipts (unchanged) ==\n");
  {
    auto g = std::make_unique<AccountStateMachine>();
    g->credit_genesis(alice_addr, 1000);
    Ledger l(ChainSpec{kChain, StateModel::kAccount}, std::move(g));  // produce_receipts = false
    Transaction t;
    t.version = 1;
    t.chain_id = kChain;
    t.body = AccountStateMachine::encode_transfer({0, bob_addr, 100});
    sign_transaction(t, alice);
    Block b = l.build_block({t}, 1);
    CHECK(b.header.version == 1);
    CHECK(l.process_block(b).ok);
    CHECK(l.last_receipts().empty());
  }

  std::printf("== UTXO receipts: fee is inputs − outputs ==\n");
  {
    auto g = std::make_unique<UtxoStateMachine>(0);
    Outpoint op0 = g->add_genesis_utxo(Output{alice_addr, 1000});
    Ledger l(receipt_spec(0, StateModel::kUtxo), std::move(g));
    // Spend 1000 → 940 out, burning a 60 fee.
    Block b = l.build_block({utxo_spend(alice, {op0}, {Output{bob_addr, 940}})}, 1);
    CHECK(b.header.version == 2);
    CHECK_MSG(l.process_block(b).ok, "utxo v2 block accepted");
    CHECK(l.last_receipts().size() == 1);
    CHECK(l.last_receipts()[0].fee == 60);
  }

  return bctest::summary();
}
