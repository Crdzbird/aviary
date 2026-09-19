// Subchains (docs/design/SUBCHAINS.md). Stage 1: types, canonical codec, the
// version-3 header fence, and genesis commitment — no anchoring behaviour yet.
#include <cstdio>

#include "blockchainer/bft_engine.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/subchain.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
crypto::ByteView v(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Hash fill(std::uint8_t x) { Hash h; h.fill(x); return h; }
crypto::KeyPair bls_key(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return crypto::bls_keygen_from_ikm(v(ikm));
}

// A real child chain: 3 BFT validators finalizing empty blocks with genuine
// certificates — exactly what a parent anchors.
struct ChildChain {
  GenesisSpec genesis;
  std::vector<crypto::KeyPair> keys;
  std::unique_ptr<consensus::BftEngine> engine;

  explicit ChildChain(std::uint64_t chain_id, std::uint8_t seed = 0x10) {
    genesis.chain_id = chain_id;
    genesis.timestamp = 1;
    for (std::uint8_t i = 0; i < 3; ++i) {
      keys.push_back(bls_key(static_cast<std::uint8_t>(seed + i)));
      genesis.validators.push_back(consensus::Validator{keys.back().public_key, 1});
    }
    engine = std::make_unique<consensus::BftEngine>(genesis.validator_set(), ledger_from_genesis(genesis));
  }
  ChildRegistration registration() const {
    ChildRegistration c;
    c.chain_id = genesis.chain_id;
    c.genesis_hash = genesis.hash();
    c.genesis_header = ledger_from_genesis(genesis)->tip();
    c.validators = genesis.validators;
    return c;
  }
  // Propose + certify (all 3 sign) WITHOUT finalizing — for equivocation tests.
  CertifiedHeader certify(std::uint64_t ts, Block* out = nullptr) {
    Block b = engine->propose({}, ts);
    std::vector<consensus::Vote> votes;
    for (std::uint32_t i = 0; i < 3; ++i) votes.push_back(engine->precommit(b, i, keys[i]));
    auto cert = engine->try_commit(b, votes);
    if (!cert) throw std::runtime_error("child: no quorum");
    if (out) *out = b;
    return CertifiedHeader{b.header, cert->encode()};
  }
  // Produce the next finalized child block.
  CertifiedHeader next(std::uint64_t ts) {
    Block b;
    CertifiedHeader ch = certify(ts, &b);
    auto cert = consensus::CommitCertificate::decode_all(v(ch.certificate));
    BlockError e = engine->finalize(b, cert);
    if (!e.ok) throw std::runtime_error("child finalize: " + e.error);
    return ch;
  }
};

BlockHeader header(std::uint8_t version, std::uint64_t chain, std::uint64_t height) {
  BlockHeader h;
  h.version = version; h.chain_id = chain; h.height = height;
  h.parent_hash = fill(0x11); h.timestamp = 7; h.tx_root = fill(0x22); h.state_root = fill(0x33);
  // Only populate what the version serializes, so a decoded copy compares equal.
  if (version >= 2) h.receipt_root = fill(0x44);
  if (version >= 3) { h.anchor_root = fill(0x55); h.subchain_root = fill(0x66); }
  h.consensus_data = Bytes{1, 2, 3};
  return h;
}
}  // namespace

int main() {
  init();

  std::printf("== I1 version fence: v1/v2 header encodings are unchanged by the new fields ==\n");
  {
    BlockHeader h1 = header(1, 9, 3), h2 = header(2, 9, 3), h3 = header(3, 9, 3);
    const std::size_t base = 1 + 8 + 8 + 32 + 8 + 32 + 32 + (4 + 3);
    CHECK(h1.encode().size() == base);
    CHECK(h2.encode().size() == base + 32);
    CHECK(h3.encode().size() == base + 32 + 64);  // anchor_root + subchain_root
    // The new fields are ignored below v3: two v2 headers differing only there hash equal.
    BlockHeader h2b = h2; h2b.anchor_root = fill(0xAA); h2b.subchain_root = fill(0xBB);
    CHECK(h2.hash() == h2b.hash());
    BlockHeader h3b = h3; h3b.subchain_root = fill(0xBB);
    CHECK(h3.hash() != h3b.hash());  // and committed at v3
    BlockHeader back = BlockHeader::decode_all(v(h3.encode()));
    CHECK(back.anchor_root == h3.anchor_root && back.subchain_root == h3.subchain_root);
    CHECK(back.encode() == h3.encode());
  }

  std::printf("== CertifiedHeader / Anchor round-trip + hash ==\n");
  Anchor anchor;
  {
    CertifiedHeader c1{header(2, 42, 1), Bytes{0xC0, 0xFF}};
    CertifiedHeader c2{header(2, 42, 2), Bytes{0xEE}};
    CHECK(CertifiedHeader::decode_all(v(c1.encode())) == c1);
    anchor.child_chain_id = 42;
    anchor.headers = {c1, c2};
    Anchor back = Anchor::decode_all(v(anchor.encode()));
    CHECK(back == anchor);
    CHECK(back.hash() == anchor.hash());
    Anchor other = anchor; other.headers[1].certificate.push_back(0);
    CHECK(other.hash() != anchor.hash());
    CHECK(compute_anchor_root({}) == crypto::zero_hash());
    CHECK(compute_anchor_root({anchor}) != crypto::zero_hash());
    CHECK(compute_anchor_root({anchor, other}) != compute_anchor_root({other, anchor}));  // order-sensitive
    CHECK_THROWS(Anchor::decode_all(v(Bytes{1, 2, 3})));
  }

  std::printf("== Block: anchors serialized only at v3; refused below ==\n");
  {
    Block b3; b3.header = header(3, 9, 1); b3.anchors = {anchor};
    Block back = Block::decode_all(v(b3.encode()));
    CHECK(back.anchors.size() == 1 && back.anchors[0] == anchor);
    CHECK(back.encode() == b3.encode());
    Block b2; b2.header = header(2, 9, 1);
    Bytes e2 = b2.encode();
    b2.anchors = {anchor};
    CHECK_THROWS(b2.encode());                 // never silently dropped
    Block b2back = Block::decode_all(v(e2));
    CHECK(b2back.anchors.empty());
    Block b3empty; b3empty.header = header(3, 9, 1);
    CHECK(Block::decode_all(v(b3empty.encode())).anchors.empty());
  }

  std::printf("== SubchainPolicy: codec, validate, genesis commitment ==\n");
  GenesisSpec parent;
  parent.chain_id = 1;
  parent.timestamp = 1;
  parent.validators = {consensus::Validator{Bytes(48, 0x01), 1}};
  const Hash plain_hash = parent.hash();
  {
    ChildRegistration c;
    c.chain_id = 42; c.genesis_hash = fill(0x42);
    c.genesis_header = header(1, 42, 0);
    c.validators = {consensus::Validator{Bytes(48, 0x02), 1}, consensus::Validator{Bytes(48, 0x03), 2}};
    ChildRegistration d = c; d.chain_id = 43; d.genesis_hash = fill(0x43); d.genesis_header.chain_id = 43;
    SubchainPolicy p;
    p.max_children = 2; p.max_depth = 2; p.max_anchors_per_block = 2; p.max_headers_per_anchor = 8;
    p.children = {c, d};
    CHECK(p.validate(1).empty());
    CHECK(SubchainPolicy::decode_all(v(p.encode())) == p);
    CHECK(SubchainPolicy::decode_all(v(p.encode())).hash() == p.hash());
    CHECK_THROWS(SubchainPolicy::decode_all(v(Bytes{0, 1})));

    // Structural rejects (each a distinct, fail-closed reason).
    SubchainPolicy bad = p; bad.children = {d, c};            CHECK(!bad.validate(1).empty());  // order
    bad = p; bad.children = {c, c};                           CHECK(!bad.validate(1).empty());  // duplicate
    bad = p; bad.max_children = 1;                            CHECK(!bad.validate(1).empty());  // too many
    bad = p; bad.max_depth = 0;                               CHECK(!bad.validate(1).empty());
    bad = p;                                                  CHECK(!bad.validate(42).empty()); // own child
    bad = p; bad.children[0].validators.clear();              CHECK(!bad.validate(1).empty());
    bad = p; bad.children[0].validators[0].power = 0;         CHECK(!bad.validate(1).empty());
    bad = p; bad.children[0].genesis_header.height = 1;       CHECK(!bad.validate(1).empty());
    bad = p; bad.children[0].genesis_header.chain_id = 7;     CHECK(!bad.validate(1).empty());

    CHECK(!SubchainPolicy::from_genesis(parent).has_value());
    p.commit_to(parent);
    CHECK(parent.hash() != plain_hash);                              // policy is chain identity
    CHECK(SubchainPolicy::from_genesis(parent).value() == p);
    GenesisSpec back = GenesisSpec::decode_all(v(parent.encode()));  // survives the genesis codec
    CHECK(back.subchain_policy == parent.subchain_policy && back.hash() == parent.hash());
  }

  std::printf("== ParentBinding: codec + child genesis commitment ==\n");
  {
    GenesisSpec child;
    child.chain_id = 42; child.timestamp = 1;
    child.validators = {consensus::Validator{Bytes(48, 0x02), 1}};
    const Hash unbound = child.hash();
    CHECK(!ParentBinding::from_genesis(child).has_value());
    ParentBinding b{1, parent.hash()};
    CHECK(ParentBinding::decode_all(v(b.encode())) == b);
    CHECK_THROWS(ParentBinding::decode_all(v(Bytes{0})));
    b.commit_to(child);
    CHECK(child.hash() != unbound);                                  // cannot be re-parented silently
    CHECK(ParentBinding::from_genesis(child).value() == b);
    ParentBinding other{1, fill(0x99)};
    GenesisSpec child2 = child; other.commit_to(child2);
    CHECK(child2.hash() != child.hash());
    CHECK(GenesisSpec::decode_all(v(child.encode())).parent_binding == child.parent_binding);
  }

  std::printf("== orthogonality: policy + binding + blueprint + membership fields are independent ==\n");
  {
    GenesisSpec g = parent;
    const Hash h0 = g.hash();
    g.parent_binding = ParentBinding{5, fill(0x05)}.encode();
    CHECK(g.hash() != h0);
    GenesisSpec back = GenesisSpec::decode_all(v(g.encode()));
    CHECK(back.subchain_policy == g.subchain_policy && back.parent_binding == g.parent_binding);
    CHECK(back.chain_blueprint.empty() && back.membership_policy.empty());
  }

  std::printf("== version fence: a chain without a policy refuses any version-3 header ==\n");
  {
    GenesisSpec plain = parent; plain.subchain_policy.clear();
    auto ledger = ledger_from_genesis(plain);
    Block b = ledger->build_block({}, 2);
    CHECK(b.header.version < 3);
    b.header.version = 3;  // same content, anchor-capable header
    BlockError e = ledger->process_block(b);
    CHECK(!e.ok && e.error.find("version-3") != std::string::npos);
    CHECK(ledger->tip().height == 0);
  }

  // ──────────────────────────────────────────────────────────────────────────
  std::printf("== STAGE 2: a parent ledger anchoring real BFT children ==\n");
  ChildChain childA(42, 0x10), childB(43, 0x20);
  GenesisSpec pgen;
  pgen.chain_id = 1;
  pgen.timestamp = 1;
  pgen.validators = {consensus::Validator{bls_key(0x01).public_key, 1}};
  SubchainPolicy policy;
  policy.max_children = 2; policy.max_depth = 1; policy.max_anchors_per_block = 2; policy.max_headers_per_anchor = 2;
  policy.children = {childA.registration(), childB.registration()};
  policy.commit_to(pgen);
  auto fresh_parent = [&] { return ledger_from_genesis(pgen); };
  auto tip_h = [](const Ledger& l, std::uint64_t child) { const BlockHeader* t = l.subchains()->tip_of(child); return t ? t->height : ~0ull; };

  std::printf("== parent genesis: v3 tip committing the initial registry; deterministic ==\n");
  {
    auto p1 = fresh_parent(); auto p2 = fresh_parent();
    CHECK(p1->tip().version == 3);
    CHECK(p1->subchains() != nullptr && p1->subchains()->children().size() == 2);
    CHECK(p1->tip().subchain_root == p1->subchains()->root());
    CHECK(p1->tip().hash() == p2->tip().hash());
    CHECK(tip_h(*p1, 42) == 0 && tip_h(*p1, 43) == 0);
    CHECK(p1->subchains()->tip_of(99) == nullptr);
    // An invalid policy is a boot failure, not a silent no-op.
    GenesisSpec bad = pgen; SubchainPolicy bp = policy; bp.children[0].chain_id = 1; bp.children[0].genesis_header.chain_id = 1; bp.commit_to(bad);
    CHECK_THROWS(ledger_from_genesis(bad));
    // A non-parent chain rejects anchors outright.
    GenesisSpec plain = pgen; plain.subchain_policy.clear();
    auto np = ledger_from_genesis(plain);
    CHECK(np->tip().version == 1 && np->subchains() == nullptr);
    Anchor a; a.child_chain_id = 42; a.headers = {childA.certify(2)};
    CHECK_THROWS(np->build_block({}, 2, {a}));
    Block vb = np->build_block({}, 2); vb.anchors = {a};
    CHECK(!np->process_block(vb).ok && np->tip().height == 0);
  }

  std::printf("== happy path: consecutive segment advances the child tip; zero-anchor block keeps the root ==\n");
  CertifiedHeader a1 = childA.next(2), a2 = childA.next(3);
  CertifiedHeader b1 = childB.next(2);
  Block good;  // parent block 1 anchoring A[1..2]
  {
    auto p = fresh_parent();
    const Hash r0 = p->tip().subchain_root;
    Anchor a; a.child_chain_id = 42; a.headers = {a1, a2};
    good = p->build_block({}, 2, {a});
    CHECK(good.header.version == 3 && good.anchors.size() == 1);
    CHECK(good.header.anchor_root == compute_anchor_root(good.anchors) && good.header.anchor_root != crypto::zero_hash());
    CHECK(good.header.subchain_root != r0);
    BlockError e = p->process_block(good);
    CHECK_MSG(e.ok, e.error.c_str());
    CHECK(p->tip().height == 1 && tip_h(*p, 42) == 2 && tip_h(*p, 43) == 0);
    CHECK(p->subchains()->tip_of(42)->hash() == a2.header.hash());
    Block empty = p->build_block({}, 3);
    CHECK(empty.header.anchor_root == crypto::zero_hash() && empty.header.subchain_root == p->tip().subchain_root);
    CHECK(p->process_block(empty).ok && p->tip().height == 2);
    // A later anchor continues from the recorded tip.
    CertifiedHeader a3 = childA.next(4);
    Anchor n; n.child_chain_id = 42; n.headers = {a3};
    Block b3 = p->build_block({}, 4, {n});
    CHECK(p->process_block(b3).ok && tip_h(*p, 42) == 3);
    // Two children in one block, canonical order.
    CertifiedHeader a4 = childA.next(5);
    Anchor na; na.child_chain_id = 42; na.headers = {a4};
    Anchor nb; nb.child_chain_id = 43; nb.headers = {b1};
    Block b4 = p->build_block({}, 5, {na, nb});
    CHECK(p->process_block(b4).ok && tip_h(*p, 42) == 4 && tip_h(*p, 43) == 1);
    CHECK_THROWS(p->build_block({}, 6, {nb, na}));  // wrong order refused by the producer too
  }

  std::printf("== rejection matrix (each on a fresh parent; ledger + registry untouched) ==\n");
  {
    auto expect_reject = [&](const char* what, Block b, const char* needle = nullptr) {
      auto p = fresh_parent();
      const Hash root0 = p->tip().subchain_root;
      BlockError e = p->process_block(b);
      CHECK_MSG(!e.ok, what);
      if (needle) CHECK_MSG(e.error.find(needle) != std::string::npos, (std::string(what) + " → " + e.error).c_str());
      CHECK_MSG(p->tip().height == 0 && p->tip().subchain_root == root0 && tip_h(*p, 42) == 0, "untouched");
    };
    auto rebuilt = [&](std::vector<Anchor> anchors) {  // consistent anchor_root, so the ANCHOR rule fires
      Block b = good; b.anchors = std::move(anchors); b.header.anchor_root = compute_anchor_root(b.anchors); return b;
    };
    Anchor A = good.anchors[0];
    { Block b = good; b.header.anchor_root = crypto::zero_hash();       expect_reject("anchor_root mismatch", b, "anchor_root"); }
    { Block b = good; b.header.subchain_root = fill(0x77);             expect_reject("subchain_root mismatch", b, "subchain_root"); }
    { Block b = good; b.header.version = 2;                            expect_reject("v2 header on a parent chain", b, "version-3"); }
    { Anchor x = A; x.child_chain_id = 99;                             expect_reject("unregistered child", rebuilt({x}), "unregistered"); }
    { Anchor x = A; x.headers[0].header.chain_id = 43;                 expect_reject("child header chain_id mismatch", rebuilt({x}), "rejected"); }
    { Anchor x = A; x.headers = {a2};                                  expect_reject("gap (skips height 1)", rebuilt({x}), "non-sequential"); }
    { Anchor x = A; x.headers = {a1, a1};                              expect_reject("repeated height", rebuilt({x}), "rejected"); }
    { Anchor x = A; x.headers.clear();                                 expect_reject("empty anchor", rebuilt({x}), "empty"); }
    { Anchor x = A; x.headers = {a1, a2, childA.certify(9)};           expect_reject("segment over max_headers_per_anchor", rebuilt({x}), "max_headers"); }
    {                                                                   expect_reject("duplicate child in block", rebuilt({A, A}), "ascending"); }
    { Anchor y; y.child_chain_id = 43; y.headers = {childB.certify(9)}; expect_reject("wrong order", rebuilt({y, A}), "ascending"); }
    { Anchor x = A; x.headers[1].certificate.back() ^= 1;              expect_reject("flipped certificate byte", rebuilt({x}), "rejected"); }
    { Anchor x = A; std::swap(x.headers[0].certificate, x.headers[1].certificate); expect_reject("certificate for another header", rebuilt({x}), "rejected"); }
    { Anchor x = A; x.headers[0].certificate.resize(3);                expect_reject("truncated certificate", rebuilt({x}), "malformed"); }
    { Anchor x = A; x.headers[1].header.state_root = fill(0xEE);       expect_reject("tampered child state_root (cert no longer matches)", rebuilt({x}), "rejected"); }
    {  // policy count bound: a parent allowing 1 anchor per block refuses 2
      GenesisSpec one = pgen; SubchainPolicy pp = policy; pp.max_anchors_per_block = 1; pp.commit_to(one);
      auto p = ledger_from_genesis(one);
      Anchor y; y.child_chain_id = 43; y.headers = {childB.certify(9)};
      CHECK_THROWS(p->build_block({}, 2, {A, y}));
      CHECK(p->tip().height == 0);
    }
    {  // a wrong validator set in the registration: genuine certs no longer verify
      GenesisSpec wrong = pgen; SubchainPolicy pp = policy;
      pp.children[0].validators[0].bls_pubkey = bls_key(0x99).public_key; pp.commit_to(wrong);
      auto p = ledger_from_genesis(wrong);
      CHECK_THROWS(p->build_block({}, 2, {A}));
    }
  }

  std::printf("== equivocation: a competing certified header at an anchored height is inert ==\n");
  {
    // Child B's validators certify TWO different blocks at height 2 (they
    // double-sign). The parent anchors whichever it sees first; the other can
    // never be anchored (height ≤ tip), so the parent's view cannot flip.
    auto p = fresh_parent();
    Anchor nb; nb.child_chain_id = 43; nb.headers = {b1};
    CHECK(p->process_block(p->build_block({}, 2, {nb})).ok && tip_h(*p, 43) == 1);
    Block fork_a, fork_b;
    CertifiedHeader h2a = childB.certify(10, &fork_a);
    CertifiedHeader h2b = childB.certify(11, &fork_b);
    CHECK(h2a.header.height == 2 && h2b.header.height == 2 && h2a.header.hash() != h2b.header.hash());
    Anchor first; first.child_chain_id = 43; first.headers = {h2a};
    CHECK(p->process_block(p->build_block({}, 3, {first})).ok && p->subchains()->tip_of(43)->hash() == h2a.header.hash());
    Anchor second; second.child_chain_id = 43; second.headers = {h2b};
    CHECK_THROWS(p->build_block({}, 4, {second}));
    Block forged = p->build_block({}, 4); forged.anchors = {second}; forged.header.anchor_root = compute_anchor_root(forged.anchors);
    BlockError e = p->process_block(forged);
    CHECK(!e.ok && p->subchains()->tip_of(43)->hash() == h2a.header.hash() && p->tip().height == 2);
    CHECK(childB.engine->finalize(fork_a, consensus::CommitCertificate::decode_all(v(h2a.certificate))).ok);
  }

  std::printf("== snapshot / restore round-trips the registry; replay is deterministic ==\n");
  {
    auto p = fresh_parent();
    CHECK(p->process_block(good).ok);
    const Bytes snap = p->snapshot();
    auto r = Ledger::restore(pgen.chain_spec(), v(snap));
    CHECK(r->tip().hash() == p->tip().hash());
    CHECK(r->subchains()->root() == p->subchains()->root() && tip_h(*r, 42) == 2);
    // A corrupted registry in the snapshot is refused (root no longer matches the tip).
    Bytes bad = snap; bad[bad.size() - 40] ^= 1;
    CHECK_THROWS(Ledger::restore(pgen.chain_spec(), v(bad)));
    // Replay on a second fresh ledger → identical roots (I8).
    auto q = fresh_parent();
    CHECK(q->process_block(good).ok);
    CHECK(q->tip().hash() == p->tip().hash() && q->subchains()->root() == p->subchains()->root());
    // A non-parent snapshot is byte-identical to before (no trailing registry).
    GenesisSpec plain = pgen; plain.subchain_policy.clear();
    auto np = ledger_from_genesis(plain);
    const Bytes plain_snap = np->snapshot();
    codec::CodecReader rd(v(plain_snap)); (void)rd.read_bytes(); (void)rd.read_bytes();
    CHECK(rd.fully_consumed());
  }

  return bctest::summary();
}
