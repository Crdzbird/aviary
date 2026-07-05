#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair bls_key(std::uint8_t s) {
  Bytes ikm(32, s);
  return crypto::bls_keygen_from_ikm(view(ikm));
}
crypto::KeyPair ed_key(std::uint8_t s) {
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
const AccountStateMachine& acc(const BftEngine& e) {
  return dynamic_cast<const AccountStateMachine&>(e.ledger().state());
}
}  // namespace

int main() {
  init();
  constexpr std::uint64_t kChain = 7;

  // 4 BLS validators (power 1 each → quorum 3).
  std::vector<crypto::KeyPair> vkeys = {bls_key(10), bls_key(11), bls_key(12), bls_key(13)};
  std::vector<Validator> vs;
  for (auto& k : vkeys) vs.push_back(Validator{k.public_key, 1});
  ValidatorSet vset(vs);

  // Account actors.
  auto alice = ed_key(1);
  auto bob = ed_key(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  auto genesis = std::make_unique<AccountStateMachine>();
  genesis->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount},
                                         std::move(genesis));
  BftEngine engine(vset, std::move(ledger));

  std::printf("== round 1: propose, 3-of-4 precommit, finalize ==\n");
  Block b1 = engine.propose({transfer(alice, bob_addr, 0, 100, kChain)}, 1);
  std::vector<Vote> votes1 = {
      engine.precommit(b1, 0, vkeys[0]),
      engine.precommit(b1, 1, vkeys[1]),
      engine.precommit(b1, 2, vkeys[2]),
  };
  auto cert1 = engine.try_commit(b1, votes1);
  CHECK(cert1.has_value());
  CHECK(engine.check_finalization(b1, *cert1).ok);
  CHECK(engine.height() == 0);  // verification does not apply
  BlockError f1 = engine.finalize(b1, *cert1);
  CHECK_MSG(f1.ok, f1.ok ? "block 1 finalized" : f1.error.c_str());
  CHECK(engine.height() == 1);
  CHECK(acc(engine).account(alice_addr).balance == 900);
  CHECK(acc(engine).account(bob_addr).balance == 100);

  std::printf("== liveness floor: 2-of-4 cannot finalize ==\n");
  {
    Block b2 = engine.propose({transfer(alice, bob_addr, 1, 50, kChain)}, 2);
    auto none = engine.try_commit(
        b2, {engine.precommit(b2, 0, vkeys[0]), engine.precommit(b2, 1, vkeys[1])});
    CHECK(!none.has_value());
    CHECK(engine.height() == 1);  // unchanged
  }

  std::printf("== adversarial: certificate cannot finalize a different block ==\n");
  {
    Block b2 = engine.propose({transfer(alice, bob_addr, 1, 50, kChain)}, 2);
    std::vector<Vote> votes2;
    for (std::uint32_t i = 0; i < 4; ++i) votes2.push_back(engine.precommit(b2, i, vkeys[i]));
    auto cert2 = engine.try_commit(b2, votes2);
    CHECK(cert2.has_value());

    // Any change to the block changes its id → the certificate no longer matches.
    Block tampered = b2;
    tampered.header.timestamp ^= 0x55;
    CHECK(!engine.check_finalization(tampered, *cert2).ok);
    CHECK(!engine.finalize(tampered, *cert2).ok);
    CHECK(engine.height() == 1);  // nothing applied

    // The genuine block finalizes.
    CHECK(engine.finalize(b2, *cert2).ok);
    CHECK(engine.height() == 2);
    CHECK(acc(engine).account(alice_addr).balance == 850);
    CHECK(acc(engine).account(bob_addr).balance == 150);
  }

  std::printf("== chain links: each block's parent is the prior header ==\n");
  CHECK(engine.tip().height == 2);

  std::printf("== drive through the abstract ConsensusEngine interface ==\n");
  {
    // The pipeline above consensus sees only ConsensusEngine: propose a block,
    // and submit it with an opaque justification (a BFT commit certificate).
    ConsensusEngine& ce = engine;
    CHECK(ce.height() == 2);
    Block b3 = ce.propose({transfer(alice, bob_addr, 2, 25, kChain)}, 3);
    auto cert3 = engine.try_commit(
        b3, {engine.precommit(b3, 0, vkeys[0]), engine.precommit(b3, 1, vkeys[1]),
             engine.precommit(b3, 2, vkeys[2])});
    CHECK(cert3.has_value());
    // The justification is the encoded certificate — engine-specific bytes.
    const Bytes justification = cert3->encode();
    CHECK_MSG(ce.submit(b3, view(justification)).ok, "submit(block, cert) commits");
    CHECK(ce.height() == 3);
    CHECK(acc(engine).account(bob_addr).balance == 175);

    // A malformed justification is rejected, chain unchanged.
    Block b4 = ce.propose({transfer(alice, bob_addr, 3, 10, kChain)}, 4);
    CHECK(!ce.submit(b4, view(Bytes{0x01, 0x02, 0x03})).ok);
    CHECK(ce.height() == 3);
  }

  return bctest::summary();
}
