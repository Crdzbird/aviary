// Subchains (docs/design/SUBCHAINS.md). Stage 1: types, canonical codec, the
// version-3 header fence, and genesis commitment — no anchoring behaviour yet.
#include <cstdio>

#include "blockchainer/genesis.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/subchain.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
crypto::ByteView v(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Hash fill(std::uint8_t x) { Hash h; h.fill(x); return h; }
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

  std::printf("== stage-1 fence: a ledger refuses any version-3 header ==\n");
  {
    auto ledger = ledger_from_genesis(parent);
    Block b = ledger->build_block({}, 2);
    CHECK(b.header.version < 3);
    b.header.version = 3;  // same content, anchor-capable header
    BlockError e = ledger->process_block(b);
    CHECK(!e.ok);
    CHECK(ledger->tip().height == 0);
  }

  return bctest::summary();
}
