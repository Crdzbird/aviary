// On-disk storage: a chain (block archive + state snapshot) is written to a
// directory with atomic, crash-safe writes, then reloaded to resume from disk.
#include <filesystem>
#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/ledger.hpp"
#include "aviary/storage.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;
using aviary::storage::FileStore;

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
const AccountStateMachine& as_account(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}
}  // namespace

int main() {
  init();
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aviary_storage_test";
  std::filesystem::remove_all(dir);  // fresh
  FileStore store(dir);

  auto alice = ed(1);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(ed(2).public_key));
  auto vkey = bls(100);
  ValidatorSet vset(std::vector<Validator>{{vkey.public_key, 1}});
  const ChainSpec spec{kChain, StateModel::kAccount};

  std::printf("== FileStore: atomic put/get/has/remove ==\n");
  {
    Bytes payload = {1, 2, 3, 4, 5};
    store.put("k", view(payload));
    CHECK(store.has("k"));
    auto got = store.get("k");
    CHECK(got.has_value() && *got == payload);
    CHECK(!store.get("absent").has_value());
    // Overwrite is atomic (old value fully replaced).
    Bytes payload2 = {9, 9};
    store.put("k", view(payload2));
    CHECK(*store.get("k") == payload2);
    store.remove("k");
    CHECK(!store.has("k"));
    // No leftover temp file.
    CHECK(!store.has("k.tmp"));
  }

  std::printf("== persist a chain, drop it, reload from disk, resume ==\n");
  Hash tip_hash;
  Hash state_root;
  {
    auto g = std::make_unique<AccountStateMachine>();
    g->credit_genesis(alice_addr, 1000);
    BftEngine engine(vset, std::make_unique<Ledger>(spec, std::move(g)));
    BlockStore blocks;
    for (std::uint64_t i = 0; i < 3; ++i) {
      Block b = engine.propose({transfer(alice, bob_addr, i, 10)}, i + 1);
      auto cert = engine.try_commit(b, {engine.precommit(b, 0, vkey)});
      CHECK(engine.finalize(b, *cert).ok);
      CHECK(blocks.put(b, *cert));
    }
    tip_hash = engine.tip().hash();
    state_root = engine.ledger().state().state_root();

    // Save to disk (files now exist on the filesystem).
    storage::save_chain(store, blocks, engine.ledger());
    CHECK(store.has(storage::kBlocksKey) && store.has(storage::kStateKey));
    // engine + blocks go out of scope here — the chain lives only on disk.
  }

  {
    auto loaded = storage::load_chain(store, spec);
    CHECK(loaded.has_value());
    // State restored: tip + root + balances match what we persisted.
    CHECK(loaded->ledger->tip().height == 3);
    CHECK(loaded->ledger->tip().hash() == tip_hash);
    CHECK(loaded->ledger->state().state_root() == state_root);
    CHECK(as_account(*loaded->ledger).account(alice_addr).balance == 970);
    CHECK(as_account(*loaded->ledger).account(bob_addr).balance == 30);
    CHECK(as_account(*loaded->ledger).account(alice_addr).nonce == 3);

    // Block archive restored: by-height, by-hash, by-txid, and certs verify.
    CHECK(loaded->blocks.size() == 3);
    CHECK(loaded->blocks.by_height(2).block.header.height == 2);
    CHECK(verify_commit(kChain, vset, loaded->blocks.by_height(1).commit).ok);

    // The reloaded ledger resumes the chain: block 4 applies.
    Block b4 = loaded->ledger->build_block({transfer(alice, bob_addr, 3, 10)}, 4);
    CHECK_MSG(loaded->ledger->process_block(b4).ok, "reloaded ledger applies block 4");
    CHECK(loaded->ledger->tip().height == 4);
  }

  std::printf("== load_chain returns nullopt for an empty store ==\n");
  {
    FileStore empty(std::filesystem::temp_directory_path() / "aviary_storage_empty");
    std::filesystem::remove_all(empty.dir());
    CHECK(!storage::load_chain(empty, spec).has_value());
  }

  std::filesystem::remove_all(dir);  // clean up
  return bctest::summary();
}
