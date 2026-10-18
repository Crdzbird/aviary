#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/ledger.hpp"
#include "aviary/mempool.hpp"
#include "aviary/signature_cache.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair ed(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::ed25519_keygen_from_seed(view(seed));
}

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce) {
  Transaction tx;
  tx.chain_id = 7;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, 5});
  sign_transaction(tx, from);
  return tx;
}

std::unique_ptr<Ledger> ledger_for(const crypto::KeyPair& alice) {
  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(crypto::address_of(view(alice.public_key)), 1000);
  return std::make_unique<Ledger>(ChainSpec{7, StateModel::kAccount}, std::move(g));
}
}

int main() {
  init();
  const auto alice = ed(1);
  const Hash bob = crypto::address_of(view(ed(2).public_key));

  std::printf("== a pass is remembered, a failure is not ==\n");
  {
    SignatureCache cache(4);
    const Transaction good = transfer(alice, bob, 0);
    const Hash leaf = tx_leaf_hash(good.encode());
    CHECK(!cache.contains(leaf));
    CHECK(cache.verify(good, leaf) && cache.contains(leaf) && cache.size() == 1);
    CHECK(cache.verify(good, leaf) && cache.size() == 1);
    Transaction bad = good;
    bad.signature[0] ^= 1;
    const Hash bad_leaf = tx_leaf_hash(bad.encode());
    CHECK_MSG(bad_leaf != leaf, "a changed signature changes the leaf");
    CHECK(!cache.verify(bad, bad_leaf) && !cache.contains(bad_leaf) && cache.size() == 1);
  }

  std::printf("== bounded: the oldest entry goes first ==\n");
  {
    SignatureCache cache(2);
    std::vector<Hash> leaves;
    for (std::uint64_t n = 0; n < 3; ++n) {
      const Transaction tx = transfer(alice, bob, n);
      leaves.push_back(tx_leaf_hash(tx.encode()));
      CHECK(cache.verify(tx, leaves.back()));
    }
    CHECK(cache.size() == 2 && !cache.contains(leaves[0]) && cache.contains(leaves[1]) && cache.contains(leaves[2]));
    CHECK(SignatureCache(0).capacity() == 1);
  }

  std::printf("== a cached transaction does not vouch for a tampered copy in a block ==\n");
  {
    auto ledger = ledger_for(alice);
    Mempool pool;
    pool.set_signature_cache(ledger->signature_cache());
    const Transaction good = transfer(alice, bob, 0);
    CHECK(pool.add(good, 7) == AddResult::kAdded);
    CHECK_MSG(ledger->signature_cache()->contains(tx_leaf_hash(good.encode())), "admission fills the ledger cache");
    Block block = ledger->build_block({good}, 2);
    Block forged = block;
    forged.transactions[0].signature[5] ^= 1;
    forged.header.tx_root = compute_tx_root(forged.transactions);
    const BlockError e = ledger->validate_block(forged);
    CHECK_MSG(!e.ok && e.error == "invalid tx signature", "tampered copy rejected");
    CHECK(ledger->process_block(block).ok);
    CHECK(ledger->tip().height == 1);
    Transaction replay = good;
    replay.signature[5] ^= 1;
    CHECK(pool.add(replay, 7) == AddResult::kInvalidSignature);
  }

  return bctest::summary();
}
