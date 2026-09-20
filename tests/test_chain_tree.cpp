// ChainTree: a declared family of chains — root ← child ← grandchild with
// different mixtures — built together (genesis bottom-up, bindings top-down),
// driven in-process (produce anchors children upward automatically), proven
// from the root, extended by subclassing, and fail-closed on a bad spec.
#include <cstdio>

#include "blockchainer/account_state.hpp"
#include "blockchainer/chain_tree.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using node::ChainTree;
using node::ChainTreeSpec;

namespace {
crypto::ByteView v(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
ChainTreeSpec::Validator val(std::uint8_t seed, std::uint64_t power = 1, bool local = true) {
  Bytes ikm(32, seed);
  crypto::KeyPair k = crypto::bls_keygen_from_ikm(v(ikm));
  return ChainTreeSpec::Validator{k.public_key, local ? k.secret_key : Bytes{}, power};
}
Hash fill(std::uint8_t x) { Hash h; h.fill(x); return h; }
}  // namespace

int main() {
  init();
  const Hash alice = fill(0xA1), bob = fill(0xB0);

  // Root: PoS/account (any engine may be a parent). Child 42: BFT, 3 validators.
  // Grandchild 420: BFT, 2 validators, alice funded. Sibling 43: BFT leaf.
  ChainTreeSpec spec;
  spec.root.chain_id = 1;
  spec.root.blueprint = ChainBlueprint{"pos", "account", "round-robin", {}};
  spec.root.validators = {val(0x01)};
  ChainTreeSpec::Node child;
  child.chain_id = 42;
  child.validators = {val(0x10), val(0x11), val(0x12)};
  child.max_headers_per_anchor = 4;
  ChainTreeSpec::Node grandchild;
  grandchild.chain_id = 420;
  grandchild.validators = {val(0x20), val(0x21)};
  grandchild.accounts = {GenesisAccount{alice, 1000}};
  child.children = {grandchild};
  ChainTreeSpec::Node sibling;
  sibling.chain_id = 43;
  sibling.validators = {val(0x30)};
  spec.root.children = {child, sibling};

  std::printf("== spec codec + validation ==\n");
  {
    CHECK(spec.validate().empty());
    ChainTreeSpec back = ChainTreeSpec::decode_all(v(spec.encode()));
    CHECK(back.root == spec.root && back.encode() == spec.encode());
    CHECK_THROWS(ChainTreeSpec::decode_all(v(Bytes{1, 2, 3})));
    ChainTreeSpec bad = spec; bad.root.children[1].chain_id = 42;            CHECK(!bad.validate().empty());  // duplicate id
    bad = spec; bad.root.children[0].validators.clear();                     CHECK(!bad.validate().empty());
    bad = spec; bad.root.children[0].blueprint.consensus_id = "pos";         CHECK(!bad.validate().empty());  // child must be BFT
    bad = spec; bad.root.children[0].validators[0].bls_public.resize(47);   CHECK(!bad.validate().empty());
    bad = spec; bad.root.children[0].validators[0].power = 0;               CHECK(!bad.validate().empty());
    CHECK_THROWS(ChainTree::build(bad));                                     // fail-closed: nothing built
    bad = spec; bad.root.blueprint.consensus_id = "no-such-engine";          CHECK_THROWS(ChainTree::build(bad));  // registry reject
    bad = spec; bad.root.assurance_level = 2;                                CHECK_THROWS(ChainTree::build(bad));  // hardened forbids pos
  }

  std::printf("== build: genesis bottom-up, bindings top-down, everything assembled ==\n");
  auto tree = ChainTree::build(spec);
  {
    CHECK(tree->root_id() == 1 && tree->chain_ids() == std::vector<std::uint64_t>({1, 42, 420, 43}));
    CHECK(tree->has(420) && !tree->has(7));
    CHECK_THROWS(tree->node(7));
    const auto& r = tree->node(1);
    CHECK(r.parent_id == 0 && r.depth == 2 && r.children == std::vector<std::uint64_t>({42, 43}));
    CHECK(tree->node(42).parent_id == 1 && tree->node(42).depth == 1 && tree->node(420).parent_id == 42 && tree->node(420).depth == 0);
    // Registrations use genesis HEADERS; bindings commit the parent's FINAL hash.
    auto rp = SubchainPolicy::from_genesis(r.genesis).value();
    CHECK(rp.children.size() == 2 && rp.children[0].chain_id == 42 && rp.children[1].chain_id == 43 && rp.max_depth == 2);
    CHECK(rp.children[0].child_max_depth == 1 && rp.children[1].child_max_depth == 0);
    CHECK(rp.children[0].genesis_header == ledger_from_genesis(tree->node(42).genesis)->tip());
    CHECK(ParentBinding::from_genesis(tree->node(42).genesis)->parent_genesis_hash == r.genesis.hash());
    CHECK(ParentBinding::from_genesis(tree->node(420).genesis)->parent_genesis_hash == tree->node(42).genesis.hash());
    CHECK(!ParentBinding::from_genesis(r.genesis));
    CHECK(tree->node(1).chain.engine->tip().version == 3 && tree->node(420).chain.engine->tip().version == 1);
    // Deterministic: the same spec builds the same identities.
    auto again = ChainTree::build(spec);
    CHECK(again->node(420).genesis.hash() == tree->node(420).genesis.hash() && again->root_tip().hash() == tree->root_tip().hash());
  }

  std::printf("== produce: blocks flow upward automatically ==\n");
  {
    CHECK(tree->pending_anchors_for(1).empty() && tree->pending_anchors_for(420).empty());
    Block g1 = tree->produce(420, 2);
    Block g2 = tree->produce(420, 3);
    CHECK(tree->height(420) == 2 && tree->node(420).archive.size() == 2);
    auto pend = tree->pending_anchors_for(42);
    CHECK(pend.size() == 1 && pend[0].child_chain_id == 420 && pend[0].headers.size() == 2);
    Block c1 = tree->produce(42, 2);                                   // anchors G[1..2]
    CHECK(c1.header.version == 3 && c1.anchors.size() == 1);
    CHECK(tree->node(42).chain.engine->ledger().subchains()->tip_of(420)->height == 2);
    CHECK(tree->pending_anchors_for(42).empty());
    Block s1 = tree->produce(43, 2);
    Block r1 = tree->produce(1, 2);                                    // PoS root anchors C[1] and S[1]
    CHECK(r1.anchors.size() == 2 && r1.anchors[0].child_chain_id == 42 && r1.anchors[1].child_chain_id == 43);
    CHECK(tree->height(1) == 1);
    const auto* reg = tree->node(1).chain.engine->ledger().subchains();
    CHECK(reg->tip_of(42)->hash() == c1.header.hash() && reg->tip_of(43)->hash() == s1.header.hash());
    // Bounded by the child's policy: 6 grandchild blocks → child anchors 4, then 2.
    for (std::uint64_t t = 4; t <= 9; ++t) tree->produce(420, t);
    CHECK(tree->pending_anchors_for(42)[0].headers.size() == 4);
    tree->produce(42, 3);
    CHECK(tree->pending_anchors_for(42)[0].headers.size() == 2);
    tree->produce(42, 4);
    CHECK(tree->pending_anchors_for(42).empty() && tree->node(42).chain.engine->ledger().subchains()->tip_of(420)->height == 8);
    tree->produce(1, 3);
    CHECK_THROWS(tree->produce(7, 3));
  }

  std::printf("== prove: a grandchild account from the root header; tamper → false ==\n");
  {
    SubchainProof p = tree->prove_account(420, alice);
    CHECK(p.hops.size() == 2 && p.hops[0].child_chain_id == 42 && p.hops[1].child_chain_id == 420);
    CHECK(tree->verify(p));
    CHECK(verify_subchain_proof(tree->root_tip().subchain_root, p));
    SubchainProof t = p; t.state_proof.leaf_preimage.back() ^= 1;
    CHECK(!tree->verify(t));
    // A root-level proof has no hops.
    SubchainProof rp = tree->prove_account(1, fill(0x77));
    CHECK(rp.hops.empty() && !tree->verify(rp));                        // absent account → not provable
    CHECK_THROWS(tree->prove_account(7, alice));
    // The root moved on since → the old proof is stale at the NEW root but valid at its own.
    const Hash old_root = tree->root_tip().subchain_root;
    tree->produce(420, 10); tree->produce(42, 5); tree->produce(1, 4);
    CHECK(!tree->verify(p) && verify_subchain_proof(old_root, p));
  }

  std::printf("== extension: a subclass supplies transactions and observes finalization ==\n");
  {
    struct Paying : ChainTree {
      crypto::KeyPair alice_key = crypto::ed25519_keygen_from_seed(v(Bytes(32, 0x5A)));
      Hash to; std::uint64_t nonce = 0; int finalized = 0;
      std::vector<Transaction> select_txs(const Built& n, std::uint64_t) override {
        if (n.chain_id != 420) return {};
        Transaction tx; tx.version = 1; tx.chain_id = 420;
        tx.body = AccountStateMachine::encode_transfer({nonce++, to, 10});
        tx.author = alice_key.public_key;
        tx.signature = crypto::ed25519_sign(v(tx.signing_bytes()), v(alice_key.secret_key));
        return {tx};
      }
      void on_finalized(const Built&, const Block&) override { ++finalized; }
      static std::unique_ptr<Paying> make(const ChainTreeSpec& s) {
        // Build the base tree, then move its state into the subclass.
        struct Access : ChainTree { using ChainTree::ChainTree; };
        auto base = ChainTree::build(s);
        auto p = std::unique_ptr<Paying>(new Paying());
        static_cast<ChainTree&>(*p) = std::move(*base);
        return p;
      }
    };
    ChainTreeSpec s2 = spec;
    const Hash alice2 = crypto::address_of(v(crypto::ed25519_keygen_from_seed(v(Bytes(32, 0x5A))).public_key));
    s2.root.children[0].children[0].accounts = {GenesisAccount{alice2, 1000}};
    auto pt = Paying::make(s2);
    pt->to = bob;
    Block g = pt->produce(420, 2);
    CHECK(g.transactions.size() == 1 && pt->finalized == 1);
    pt->produce(42, 2); pt->produce(1, 2);
    CHECK(pt->finalized == 3);
    SubchainProof bp = pt->prove_account(420, bob);
    CHECK(pt->verify(bp));
  }

  return bctest::summary();
}
