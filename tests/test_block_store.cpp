// Persistent, indexed block archive: contiguous-by-height storage of finalized
// blocks + commit certs, with by-hash and by-txid lookup, serializable so a node
// can reload its chain on restart.
#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/ledger.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
constexpr std::uint64_t kChain = 7;

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce, codec::u128 amt) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}
}  // namespace

int main() {
  init();
  auto alice = ed(1);
  auto vkey = bls(100);
  ValidatorSet vset(std::vector<Validator>{{vkey.public_key, 1}});  // quorum 1
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(ed(2).public_key));

  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
  BftEngine engine(vset, std::move(ledger));

  // Finalize three blocks and archive each with its commit certificate.
  BlockStore store;
  std::vector<Hash> block_hashes, txids;
  for (std::uint64_t i = 0; i < 3; ++i) {
    Transaction t = transfer(alice, bob_addr, i, 10);
    Block b = engine.propose({t}, i + 1);
    auto cert = engine.try_commit(b, {engine.precommit(b, 0, vkey)});
    CHECK(cert.has_value());
    CHECK(engine.finalize(b, *cert).ok);
    CHECK(store.put(b, *cert));
    block_hashes.push_back(b.header.hash());
    txids.push_back(t.txid());
  }

  std::printf("== indexed lookups by height, hash, and txid ==\n");
  CHECK(store.size() == 3);
  CHECK(store.tip_height() == 3);
  CHECK(store.by_height(2).block.header.hash() == block_hashes[1]);
  CHECK(store.by_hash(block_hashes[2]).block.header.height == 3);
  CHECK(store.has_height(1) && !store.has_height(4));

  auto loc = store.find_tx(txids[1]);
  CHECK(loc.has_value());
  CHECK(loc->height == 2 && loc->index == 0);
  CHECK(!store.find_tx(Hash{}).has_value());  // unknown tx

  std::printf("== put rejects non-contiguous or duplicate heights ==\n");
  {
    // Re-putting height 3 is a duplicate; skipping to height 5 is non-contiguous.
    const StoredBlock& sb3 = store.by_height(3);
    CHECK(!store.put(sb3.block, sb3.commit));  // duplicate
    Block gap = sb3.block;
    gap.header.height = 5;
    CHECK(!store.put(gap, sb3.commit));  // non-contiguous
    CHECK(store.size() == 3);            // unchanged
  }

  std::printf("== serialize -> load rebuilds blocks + every index ==\n");
  {
    Bytes blob = store.serialize();
    BlockStore back = BlockStore::load(view(blob));
    CHECK(back.size() == 3);
    CHECK(back.tip_height() == 3);
    // by-hash and by-txid indexes are rebuilt, not persisted.
    CHECK(back.by_hash(block_hashes[0]).block.header.height == 1);
    auto l = back.find_tx(txids[2]);
    CHECK(l.has_value() && l->height == 3);
    // The stored commit certificate still verifies.
    CHECK(verify_commit(kChain, vset, back.by_height(2).commit).ok);
    // Round-trip is stable.
    CHECK(back.serialize() == blob);
  }

  return bctest::summary();
}
