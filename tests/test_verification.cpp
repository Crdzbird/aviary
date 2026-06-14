// Light client + inclusion proofs: verify a header chain without state, then
// verify account and transaction inclusion against the verified roots.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/light_client.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;
using blockchainer::client::LightClient;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
constexpr std::uint64_t kChain = 7;
Transaction transfer(const crypto::KeyPair& f, const Hash& to, std::uint64_t n, codec::u128 a) {
  Transaction tx; tx.version = 1; tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({n, to, a});
  sign_transaction(tx, f);
  return tx;
}
const AccountStateMachine& acc(const BftEngine& e) {
  return dynamic_cast<const AccountStateMachine&>(e.ledger().state());
}
}  // namespace

int main() {
  init();

  auto vkey = bls(100);
  ValidatorSet vset(std::vector<Validator>{{vkey.public_key, 1}});
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
  BftEngine engine(vset, std::move(ledger));

  const BlockHeader genesis = engine.tip();  // trusted root for the light client

  // Produce + finalize a block with two transfers.
  Block blk = engine.propose({transfer(alice, bob_addr, 0, 100), transfer(alice, bob_addr, 1, 50)}, 1);
  auto cert = engine.try_commit(blk, {engine.precommit(blk, 0, vkey)});
  CHECK(engine.finalize(blk, *cert).ok);

  std::printf("== light client verifies the header chain (no state) ==\n");
  LightClient lc(kChain, vset, genesis);
  CHECK(lc.height() == 0);
  LightClient::Result ar = lc.accept(blk.header, *cert);
  CHECK_MSG(ar.ok, ar.ok ? "header accepted" : ar.error.c_str());
  CHECK(lc.height() == 1);
  CHECK(lc.tip().state_root == blk.header.state_root);  // trusts the root via the cert

  std::printf("== light client rejects bad inputs ==\n");
  {
    auto bad_cert = *cert;
    bad_cert.agg_signature[0] ^= 0x01;
    LightClient lc2(kChain, vset, genesis);
    CHECK(!lc2.accept(blk.header, bad_cert).ok);          // bad signature
    // Wrong parent linkage.
    BlockHeader orphan = blk.header;
    orphan.parent_hash[0] ^= 0x01;
    LightClient lc3(kChain, vset, genesis);
    CHECK(!lc3.accept(orphan, *cert).ok);
  }

  std::printf("== account inclusion proof against the verified state_root ==\n");
  {
    // The light client holds no state; a full node serves a proof, the light
    // client verifies it against the root it already trusts.
    StateProof p = acc(engine).prove_account(alice_addr);
    CHECK(p.present);
    CHECK(verify_state_proof(lc.tip().state_root, p));
    // The committed value is readable and correct (alice spent 150 → 850).
    auto leaf = AccountStateMachine::decode_account_leaf(p.leaf_preimage);
    CHECK(leaf.address == alice_addr);
    CHECK(leaf.balance == 850);
    CHECK(leaf.nonce == 2);

    // Tampered proof / wrong root rejected.
    StateProof tampered = p;
    tampered.leaf_preimage[40] ^= 0x01;  // corrupt the balance bytes
    CHECK(!verify_state_proof(lc.tip().state_root, tampered));
    CHECK(!verify_state_proof(genesis.state_root, p));  // wrong (earlier) root

    // Round-trips through canonical encoding.
    StateProof rt = StateProof::decode_all(view(p.encode()));
    CHECK(verify_state_proof(lc.tip().state_root, rt));

    // Absence proof is not verifiable (present=false).
    StateProof none = acc(engine).prove_account(crypto::address_of(view(ed(9).public_key)));
    CHECK(!none.present);
    CHECK(!verify_state_proof(lc.tip().state_root, none));
  }

  std::printf("== transaction inclusion proof against tx_root ==\n");
  {
    for (std::uint32_t i = 0; i < blk.transactions.size(); ++i) {
      TxInclusionProof tp = tx_inclusion_proof(blk, i);
      CHECK(verify_tx_inclusion(blk.header.tx_root, tp));
      TxInclusionProof rt = TxInclusionProof::decode_all(view(tp.encode()));
      CHECK(verify_tx_inclusion(blk.header.tx_root, rt));
    }
    // Tampered tx bytes rejected.
    TxInclusionProof tp = tx_inclusion_proof(blk, 0);
    tp.tx_encoding[0] ^= 0x01;
    CHECK(!verify_tx_inclusion(blk.header.tx_root, tp));
  }

  return bctest::summary();
}
