#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

#include "aviary/assurance.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/chain_snapshot.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/version.hpp"
#include "check.hpp"
#include "dynamic_support.hpp"

using namespace aviary;
using namespace dyntest;

namespace {
DynamicSubchainPolicy base_policy(const std::vector<const crypto::KeyPair*>& registrars, std::uint8_t threshold) {
  DynamicSubchainPolicy p;
  p.max_depth = 2;
  p.authority.threshold = threshold;
  for (const crypto::KeyPair* k : registrars) p.authority.registrars.push_back(ed_registrar(*k));
  return p;
}

BlockHeader header(std::uint8_t version) {
  BlockHeader h;
  h.version = version;
  h.chain_id = 9;
  h.height = 3;
  h.parent_hash = fill(0x11);
  h.timestamp = 7;
  h.tx_root = fill(0x22);
  h.state_root = fill(0x33);
  if (version >= 2) h.receipt_root = fill(0x44);
  if (version >= 3) {
    h.anchor_root = fill(0x55);
    h.subchain_root = fill(0x66);
  }
  if (version >= 4) h.ops_root = fill(0x77);
  h.consensus_data = Bytes{1, 2, 3};
  return h;
}

std::string op_error(const Parent& p, std::uint64_t height, const SubchainOp& op) {
  DynamicSubchainRegistry copy = p.reg();
  return copy.apply(height, {op}, {});
}
}  // namespace

int main() {
  init();
  const crypto::KeyPair r0 = ed(0xA0), r1 = ed(0xA1), r2 = ed(0xA2), stranger = ed(0xAF);

  std::printf("== I1 / I13 encodings: versions 1 to 3 unchanged, version 4 adds ops_root ==\n");
  {
    const std::size_t base = 1 + 8 + 8 + 32 + 8 + 32 + 32 + (4 + 3);
    CHECK(header(1).encode().size() == base);
    CHECK(header(2).encode().size() == base + 32);
    CHECK(header(3).encode().size() == base + 32 + 64);
    CHECK(header(4).encode().size() == base + 32 + 64 + 32);
    for (std::uint8_t ver = 1; ver <= 3; ++ver) {
      BlockHeader a = header(ver), b = header(ver);
      b.ops_root = fill(0xEE);
      CHECK(a.encode() == b.encode() && a.hash() == b.hash());
    }
    BlockHeader a = header(4), b = header(4);
    b.ops_root = fill(0xEE);
    CHECK(a.hash() != b.hash());
    BlockHeader back = BlockHeader::decode_all(v(a.encode()));
    CHECK(back == a && back.encode() == a.encode());
    Bytes as_v3 = header(3).encode();
    as_v3[0] = 4;
    CHECK_THROWS(BlockHeader::decode_all(v(as_v3)));

    Block blk;
    blk.header = header(4);
    SubchainOp op = make_close_op(0, 9, CloseChild{42, 5, fill(0x01)});
    add_op_approval(op, 2, Bytes{9, 9});
    add_op_approval(op, 0, Bytes{8});
    CHECK(op.approvals[0].registrar_index == 0 && op.approvals[1].registrar_index == 2);
    CHECK_THROWS(add_op_approval(op, 2, Bytes{1}));
    blk.ops = {op};
    Block round = Block::decode_all(v(blk.encode()));
    CHECK(round.ops == blk.ops && round.encode() == blk.encode());
    Block lower;
    lower.header = header(3);
    lower.ops = {op};
    CHECK_THROWS(lower.encode());
    CHECK(compute_ops_root({}) == crypto::zero_hash() && compute_ops_root({op}) == op.hash());
    SubchainOp other = op;
    other.valid_until = 10;
    CHECK(compute_ops_root({op, other}) != compute_ops_root({other, op}));
  }

  std::printf("== operation codec: strict, approvals ascending and unique ==\n");
  {
    SubchainOp op = make_register_op(3, 77, Child(42, 0x10).registration());
    add_op_approval(op, 1, Bytes(64, 0x01));
    add_op_approval(op, 4, Bytes(64, 0x02));
    CHECK(SubchainOp::decode_all(v(op.encode())) == op);
    Bytes trailing = op.encode();
    trailing.push_back(0);
    CHECK_THROWS(SubchainOp::decode_all(v(trailing)));
    SubchainOp swapped = op;
    std::swap(swapped.approvals[0], swapped.approvals[1]);
    CHECK_THROWS(SubchainOp::decode_all(v(swapped.encode())));
    SubchainOp repeated = op;
    repeated.approvals[1].registrar_index = 1;
    CHECK_THROWS(SubchainOp::decode_all(v(repeated.encode())));
    const Hash g = fill(0x77);
    CHECK(op.auth_digest(1, g) != op.auth_digest(2, g));
    CHECK(op.auth_digest(1, g) != op.auth_digest(1, fill(0x78)));
    SubchainOp unsigned_op = op;
    unsigned_op.approvals.clear();
    CHECK(op.auth_digest(1, g) == unsigned_op.auth_digest(1, g) && op.hash() != unsigned_op.hash());
    SubchainOp later = op;
    later.authority_epoch = 4;
    CHECK(later.auth_digest(1, g) != op.auth_digest(1, g));
    later = op;
    later.valid_until = 78;
    CHECK(later.auth_digest(1, g) != op.auth_digest(1, g));
    later = op;
    later.kind = SubchainOp::kClose;
    CHECK(later.auth_digest(1, g) != op.auth_digest(1, g));
    CloseChild close{42, 9, fill(0x05)};
    CHECK(CloseChild::decode_all(v(close.encode())) == close);
    Bytes long_close = close.encode();
    long_close.push_back(1);
    CHECK_THROWS(CloseChild::decode_all(v(long_close)));
  }

  std::printf("== policy codec, format detection and validation ==\n");
  {
    Child a(42, 0x10), b(43, 0x11);
    DynamicSubchainPolicy p = base_policy({&r0, &r1, &r2}, 2);
    p.max_children = 10;
    p.max_anchors_per_block = 4;
    p.max_headers_per_anchor = 8;
    p.max_ops_per_block = 16;
    p.children = {a.registration(), b.registration(1)};
    const Bytes enc = p.encode();
    CHECK(enc[0] == 0 && enc[1] == 0 && enc[2] == 0 && enc[3] == 2);
    CHECK(DynamicSubchainPolicy::decode_all(v(enc)) == p);
    CHECK(p.validate(1).empty());
    CHECK(subchain_policy_format(v(enc)) == 2 && subchain_policy_format({}) == 0);
    Bytes trailing = enc;
    trailing.push_back(0);
    CHECK_THROWS(DynamicSubchainPolicy::decode_all(v(trailing)));
    Bytes other_format = enc;
    other_format[3] = 3;
    CHECK(subchain_policy_format(v(other_format)) == 3);
    CHECK_THROWS(DynamicSubchainPolicy::decode_all(v(other_format)));
    other_format[3] = 1;
    CHECK(subchain_policy_format(v(other_format)) == 0xFF);

    SubchainPolicy flat;
    flat.max_children = 4;
    flat.max_depth = 1;
    flat.children = {a.registration()};
    CHECK(subchain_policy_format(v(flat.encode())) == 1);
    bool flat_reading_rejected = false;
    try {
      flat_reading_rejected = !SubchainPolicy::decode_all(v(enc)).validate(1).empty();
    } catch (const std::exception&) {
      flat_reading_rejected = true;
    }
    CHECK_MSG(flat_reading_rejected, "a dynamic policy can never be taken for a valid version 1 policy");
    CHECK(p.hash() != flat.hash());

    GenesisSpec g;
    g.chain_id = 1;
    p.commit_to(g);
    CHECK(DynamicSubchainPolicy::from_genesis(g).value() == p);
    GenesisSpec g1;
    g1.chain_id = 1;
    flat.commit_to(g1);
    CHECK(!DynamicSubchainPolicy::from_genesis(g1).has_value());

    DynamicSubchainPolicy bad = p;
    bad.max_depth = 0;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.threshold = 0;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.threshold = 4;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars[1] = bad.authority.registrars[0];
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars[0].sig_algo = 3;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars[0].sig_algo = 9;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars[0].public_key.pop_back();
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars[0] = Registrar{2, Bytes{}};
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.authority.registrars.clear();
    CHECK(!bad.validate(1).empty());
    bad.authority.threshold = 0;
    CHECK(bad.validate(1).empty());
    bad = p;
    bad.authority.registrars.assign(65, ed_registrar(r0));
    for (std::size_t i = 0; i < bad.authority.registrars.size(); ++i)
      bad.authority.registrars[i].public_key[0] = static_cast<std::uint8_t>(i);
    CHECK(!bad.validate(1).empty());
    bad = p;
    std::swap(bad.children[0], bad.children[1]);
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.children[1].chain_id = 42;
    CHECK(!bad.validate(1).empty());
    bad = p;
    CHECK(!bad.validate(42).empty());
    bad = p;
    bad.children[0].child_max_depth = 2;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.max_children = 1;
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.children[0].validators.clear();
    CHECK(!bad.validate(1).empty());
    bad = p;
    bad.children[0].genesis_header.height = 1;
    CHECK(!bad.validate(1).empty());
  }

  std::printf("== boot: a dynamic parent starts at a version 4 genesis header and needs its genesis hash ==\n");
  {
    Child a(42, 0x10);
    DynamicSubchainPolicy p = base_policy({&r0}, 1);
    p.children = {a.registration()};
    Parent parent(1, p);
    CHECK(parent.tip().version == 4 && parent.tip().height == 0);
    CHECK(parent.tip().ops_root == crypto::zero_hash() && parent.tip().anchor_root == crypto::zero_hash());
    CHECK(parent.tip().subchain_root == parent.reg().root() && parent.reg().root() == naive_root(parent.reg()));
    CHECK(parent.reg().record_count() == 1 && parent.reg().open_count() == 1 && parent.reg().authority_epoch() == 0);
    CHECK(parent.engine->ledger().subchains() == nullptr);
    ChainSpec spec = parent.genesis.chain_spec();
    CHECK(spec.genesis_hash == parent.genesis_hash);
    spec.genesis_hash = Hash{};
    CHECK_THROWS(Ledger(spec, parent.genesis.build_state(), 1));
    spec = parent.genesis.chain_spec();
    spec.subchain_policy[3] = 3;
    CHECK_THROWS(Ledger(spec, parent.genesis.build_state(), 1));
    DynamicSubchainPolicy invalid = p;
    invalid.max_depth = 0;
    spec.subchain_policy = invalid.encode();
    CHECK_THROWS(Ledger(spec, parent.genesis.build_state(), 1));

    DynamicSubchainPolicy empty = base_policy({&r0}, 1);
    Parent bare(2, empty);
    CHECK(bare.reg().record_count() == 0 && bare.tip().subchain_root == naive_root(bare.reg()));
  }

  std::printf("== I13 version fence, exact per policy format ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    Block good = parent.engine->propose({}, 5, {}, {});
    CHECK(good.header.version == 4 && parent.engine->ledger().validate_block(good).ok);
    for (std::uint8_t ver : {std::uint8_t{1}, std::uint8_t{2}, std::uint8_t{3}, std::uint8_t{5}}) {
      Block b = good;
      b.header.version = ver;
      CHECK(!parent.engine->ledger().validate_block(b).ok);
    }

    Child a(42, 0x10);
    SubchainPolicy flat;
    flat.max_children = 2;
    flat.max_depth = 1;
    flat.children = {a.registration()};
    GenesisSpec g;
    g.chain_id = 3;
    g.timestamp = 1;
    flat.commit_to(g);
    auto v3 = ledger_from_genesis(g);
    Block b3 = v3->build_block({}, 5);
    CHECK(b3.header.version == 3 && v3->validate_block(b3).ok);
    Block b4 = b3;
    b4.header.version = 4;
    CHECK(contains(v3->validate_block(b4).error, "exactly version-3"));
    CHECK_THROWS(v3->build_block({}, 5, {}, {make_close_op(0, 0, CloseChild{42, 0, fill(0)})}));

    GenesisSpec plain;
    plain.chain_id = 4;
    plain.timestamp = 1;
    auto none = ledger_from_genesis(plain);
    Block b1 = none->build_block({}, 5);
    CHECK(b1.header.version == 1);
    Block up = b1;
    up.header.version = 4;
    CHECK(!none->validate_block(up).ok);
    CHECK_THROWS(none->build_block({}, 5, {}, {make_close_op(0, 0, CloseChild{42, 0, fill(0)})}));
  }

  std::printf("== register after genesis, anchor, prove ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    Child a(42, 0x10), b(43, 0x11);
    const Hash genesis_root = parent.reg().root();
    SubchainOp reg_a = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}});
    Block b1 = parent.produce(10, {}, {reg_a});
    CHECK(b1.ops.size() == 1 && b1.header.ops_root == compute_ops_root(b1.ops));
    CHECK(parent.reg().record_count() == 1 && parent.reg().open_count() == 1);
    CHECK(parent.reg().root() != genesis_root && parent.reg().root() == naive_root(parent.reg()));
    CHECK(parent.tip().subchain_root == parent.reg().root());
    CHECK(parent.follower->dynamic_subchains()->root() == parent.reg().root());
    const ChildRecord rec = parent.reg().child(42)->record;
    CHECK(rec.index == 0 && rec.status == ChildRecord::kOpen && rec.registered_height == 1 && rec.tip_height == 0);
    CHECK(rec.genesis_header_hash == a.registration().genesis_header.hash() && rec.tip_hash == rec.genesis_header_hash);
    CHECK(rec.encode().size() == ChildRecord::kEncodedSize);
    CHECK(ChildRecord::decode_all(v(rec.encode())) == rec);

    a.next(20);
    a.next(21);
    SubchainOp reg_b = parent.sign(make_register_op(0, 0, b.registration()), {{0, &r0}});
    b.next(30);
    Block b2 = parent.produce(11, parent.anchors_for({&a}), {reg_b});
    CHECK(b2.anchors.size() == 1 && parent.reg().child(42)->record.tip_height == 2);
    CHECK(parent.reg().child(43)->record.index == 1 && parent.reg().child(43)->record.registered_height == 2);
    CHECK(parent.reg().root() == naive_root(parent.reg()));

    Child c(44, 0x12);
    c.next(40);
    SubchainOp reg_c = parent.sign(make_register_op(0, 0, c.registration()), {{0, &r0}});
    DynamicSubchainRegistry view = parent.reg();
    auto pending_c = node::pending_anchor(view, 44, c.archive);
    CHECK(!pending_c.has_value());
    Anchor first_c;
    first_c.child_chain_id = 44;
    first_c.headers = {CertifiedHeader{c.archive.by_height(1).block.header, c.archive.by_height(1).commit.encode()}};
    std::vector<Anchor> same_block = parent.anchors_for({&b});
    same_block.push_back(first_c);
    parent.produce(12, same_block, {reg_c});
    CHECK(parent.reg().child(44)->record.tip_height == 1 && parent.reg().child(43)->record.tip_height == 1);

    const ChildProof proof = parent.reg().prove(42);
    const std::optional<ChildRecord> proven = verify_child_proof(parent.tip().subchain_root, proof);
    CHECK(proven.has_value() && *proven == parent.reg().child(42)->record);
    CHECK(proven->tip_hash == a.engine->tip().hash() && proven->tip_state_root == a.engine->tip().state_root);
    CHECK(ChildProof::decode_all(v(proof.encode())) == proof);
    CHECK(!verify_child_proof(b2.header.subchain_root, proof).has_value());
    CHECK_THROWS(parent.reg().prove(99));
    CHECK(parent.reg().tip_of(99) == nullptr && parent.reg().child_at(7) == nullptr);
  }

  std::printf("== I20 proof soundness: every field, sibling and outer value is bound ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    std::vector<std::unique_ptr<Child>> kids;
    std::vector<SubchainOp> ops;
    for (std::uint64_t i = 0; i < 7; ++i) {
      kids.push_back(std::make_unique<Child>(100 + i, static_cast<std::uint8_t>(0x20 + i)));
      ops.push_back(parent.sign(make_register_op(0, 0, kids.back()->registration()), {{0, &r0}}));
    }
    parent.produce(10, {}, ops);
    const Hash root = parent.tip().subchain_root;
    for (std::uint64_t i = 0; i < 7; ++i) {
      const ChildProof proof = parent.reg().prove(100 + i);
      CHECK(verify_child_proof(root, proof).has_value());
      const merkle::Proof plain = merkle::make_proof(
          [&] {
            std::vector<Hash> leaves;
            for (std::uint32_t k = 0; k < 7; ++k) leaves.push_back(parent.reg().child_at(k)->record.leaf());
            return leaves;
          }(),
          static_cast<std::uint32_t>(i));
      CHECK(plain.siblings == proof.siblings);
    }
    const ChildProof good = parent.reg().prove(103);
    auto rejected = [&](auto&& mutate) {
      ChildProof bad = good;
      mutate(bad);
      return !verify_child_proof(root, bad).has_value();
    };
    CHECK(rejected([](ChildProof& p) { p.record.index ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.chain_id += 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.status = ChildRecord::kClosed; }));
    CHECK(rejected([](ChildProof& p) {
      p.record.status = ChildRecord::kClosed;
      p.record.closed_height = 1;
    }));
    CHECK(rejected([](ChildProof& p) { p.record.status = 3; }));
    CHECK(rejected([](ChildProof& p) { p.record.child_max_depth += 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.genesis_header_hash[0] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.registered_height += 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.tip_height += 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.tip_hash[31] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.tip_state_root[5] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.tip_subchain_root[5] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.vset_hash[5] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record.tip_vset_hash[5] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.record_count += 1; }));
    CHECK(rejected([](ChildProof& p) { p.record_count = 4; }));
    CHECK(rejected([](ChildProof& p) { p.open_count -= 1; }));
    CHECK(rejected([](ChildProof& p) { p.authority_epoch += 1; }));
    CHECK(rejected([](ChildProof& p) { p.authority_hash[0] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.siblings[0][0] ^= 1; }));
    CHECK(rejected([](ChildProof& p) { p.siblings.pop_back(); }));
    CHECK(rejected([](ChildProof& p) { p.siblings.push_back(Hash{}); }));
    CHECK(rejected([](ChildProof& p) { std::swap(p.siblings[0], p.siblings[1]); }));
    ChildProof last = parent.reg().prove(106);
    CHECK(last.siblings.size() == 2 && verify_child_proof(root, last).has_value());
    last.record_count = 8;
    CHECK(!verify_child_proof(root, last).has_value());
    CHECK(!verify_child_proof(fill(0x00), good).has_value());

    Bytes bad_record = good.record.encode();
    bad_record[12] = 0;
    CHECK_THROWS(ChildRecord::decode_all(v(bad_record)));
    bad_record = good.record.encode();
    bad_record[4 + 8 + 1 + 1 + 32 + 8 + 7] = 1;
    CHECK_THROWS(ChildRecord::decode_all(v(bad_record)));
    Bytes long_proof = good.encode();
    long_proof.push_back(0);
    CHECK_THROWS(ChildProof::decode_all(v(long_proof)));
  }

  std::printf("== I14 the operation validation matrix, one check per rule ==\n");
  {
    DynamicSubchainPolicy p = base_policy({&r0, &r1, &r2}, 2);
    p.max_children = 2;
    p.max_depth = 1;
    Parent parent(1, p);
    Child a(42, 0x10), b(43, 0x11), c(44, 0x12);
    const auto both = std::vector<std::pair<std::uint16_t, const crypto::KeyPair*>>{{0, &r0}, {2, &r2}};
    const SubchainOp good = parent.sign(make_register_op(0, 0, a.registration()), both);
    CHECK(op_error(parent, 1, good).empty());

    SubchainOp op = good;
    op.kind = 9;
    CHECK(contains(op_error(parent, 1, op), "unknown kind"));
    op = parent.sign(make_register_op(1, 0, a.registration()), both);
    CHECK(contains(op_error(parent, 1, op), "epoch"));
    op = parent.sign(make_register_op(0, 5, a.registration()), both);
    CHECK(op_error(parent, 5, op).empty() && contains(op_error(parent, 6, op), "expired"));
    op = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}});
    CHECK(contains(op_error(parent, 1, op), "not enough approvals"));
    op = make_register_op(0, 0, a.registration());
    CHECK(contains(op_error(parent, 1, op), "not enough approvals"));
    op = good;
    std::swap(op.approvals[0], op.approvals[1]);
    CHECK(contains(op_error(parent, 1, op), "ascending"));
    op = good;
    op.approvals[1].registrar_index = 0;
    CHECK(contains(op_error(parent, 1, op), "ascending"));
    op = good;
    op.approvals[1].registrar_index = 3;
    CHECK(contains(op_error(parent, 1, op), "unknown registrar"));
    op = good;
    op.approvals[1].signature[10] ^= 1;
    CHECK(contains(op_error(parent, 1, op), "bad registrar signature"));
    op = good;
    op.approvals[1].signature.pop_back();
    CHECK(contains(op_error(parent, 1, op), "bad registrar signature"));
    op = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}, {2, &stranger}});
    CHECK(contains(op_error(parent, 1, op), "bad registrar signature"));
    op = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}, {1, &r2}});
    CHECK(contains(op_error(parent, 1, op), "bad registrar signature"));
    op = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}, {1, &r1}, {2, &r2}});
    CHECK(op_error(parent, 1, op).empty());
    op.approvals[2].signature[0] ^= 1;
    CHECK_MSG(contains(op_error(parent, 1, op), "bad registrar signature"),
              "an extra approval beyond the threshold must still be valid");
    op = good;
    op.valid_until = 7;
    CHECK_MSG(contains(op_error(parent, 1, op), "bad registrar signature"), "valid_until is signed");
    op = good;
    op.payload.push_back(0);
    CHECK(contains(op_error(parent, 1, op), "malformed payload"));
    op = good;
    op.payload.resize(5);
    CHECK(contains(op_error(parent, 1, op), "malformed payload"));

    ChildRegistration bad = a.registration();
    bad.chain_id = 1;
    bad.genesis_header.chain_id = 1;
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "its own child"));
    bad = a.registration();
    bad.validators.clear();
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "no validators"));
    bad = a.registration();
    bad.validators[0].power = 0;
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "zero power"));
    bad = a.registration();
    bad.validators[0].bls_pubkey.pop_back();
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "malformed payload"));
    bad = a.registration();
    bad.validators.push_back(bad.validators[0]);
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "malformed payload"));
    bad = a.registration();
    bad.genesis_header.height = 1;
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "height 0"));
    bad = a.registration();
    bad.genesis_header.chain_id = 77;
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "chain_id mismatch"));
    bad = a.registration(1);
    CHECK(contains(op_error(parent, 1, parent.sign(make_register_op(0, 0, bad), both)), "deeper"));

    const Hash before = parent.reg().root();
    const BlockHeader tip_before = parent.tip();
    SubchainOp forged = good;
    forged.approvals[0].signature[3] ^= 1;
    CHECK(contains(parent.refuses(10, {}, {forged}), "bad registrar signature"));
    Block valid = parent.engine->propose({}, 10, {}, {good});
    Block tampered = valid;
    tampered.ops[0] = forged;
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "ops_root mismatch"));
    tampered.header.ops_root = compute_ops_root(tampered.ops);
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "bad registrar signature"));
    tampered = valid;
    tampered.ops.clear();
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "ops_root mismatch"));
    tampered.header.ops_root = crypto::zero_hash();
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "subchain_root mismatch"));
    tampered = valid;
    tampered.header.subchain_root[0] ^= 1;
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "subchain_root mismatch"));
    tampered = valid;
    tampered.ops.push_back(good);
    tampered.header.ops_root = compute_ops_root(tampered.ops);
    CHECK(contains(parent.engine->ledger().validate_block(tampered).error, "already registered"));
    CHECK_MSG(parent.reg().root() == before && parent.tip() == tip_before && parent.reg().record_count() == 0,
              "I7: a refused block changes nothing");

    std::printf("-- I17 unique forever, max_children counts open children, ops per block --\n");
    parent.produce(10, {}, {good});
    CHECK(contains(op_error(parent, 2, good), "already registered"));
    parent.produce(11, {}, {parent.sign(make_register_op(0, 0, b.registration()), both)});
    const SubchainOp third = parent.sign(make_register_op(0, 0, c.registration()), both);
    CHECK(contains(op_error(parent, 3, third), "max_children"));
    const ChildRecord arec = parent.reg().child(42)->record;
    const SubchainOp close_a = parent.sign(make_close_op(0, 0, CloseChild{42, arec.tip_height, arec.tip_hash}), both);
    parent.produce(12, {}, {close_a, third});
    CHECK(parent.reg().open_count() == 2 && parent.reg().record_count() == 3);
    CHECK(contains(op_error(parent, 4, good), "already registered"));
    CHECK(parent.reg().root() == naive_root(parent.reg()));

    DynamicSubchainPolicy tight = base_policy({&r0}, 1);
    tight.max_ops_per_block = 1;
    tight.max_anchors_per_block = 1;
    Parent small(5, tight);
    Child x(50, 0x30), y(51, 0x31);
    const SubchainOp rx = small.sign(make_register_op(0, 0, x.registration()), {{0, &r0}});
    const SubchainOp ry = small.sign(make_register_op(0, 0, y.registration()), {{0, &r0}});
    CHECK(contains(small.refuses(10, {}, {rx, ry}), "too many registry operations"));
    small.produce(10, {}, {rx});
    small.produce(11, {}, {ry});
    x.next(20);
    y.next(20);
    CHECK(contains(small.refuses(12, small.anchors_for({&x, &y}), {}), "too many anchors"));
  }

  std::printf("== I15 an operation signed for one parent is invalid on any other ==\n");
  {
    const DynamicSubchainPolicy p = base_policy({&r0}, 1);
    Parent production(1, p, 1), staging(1, p, 2), sibling(2, p, 1);
    CHECK(production.genesis_hash != staging.genesis_hash);
    Child a(42, 0x10);
    const SubchainOp op = production.sign(make_register_op(0, 0, a.registration()), {{0, &r0}});
    CHECK(op_error(production, 1, op).empty());
    CHECK(contains(op_error(staging, 1, op), "bad registrar signature"));
    CHECK(contains(op_error(sibling, 1, op), "bad registrar signature"));
  }

  std::printf("== closing: I18 final, the record stays provable, no anchor, no reuse ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    Child a(42, 0x10), b(43, 0x11);
    parent.produce(10, {},
                   {parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}}),
                    parent.sign(make_register_op(0, 0, b.registration()), {{0, &r0}})});
    a.next(20);
    a.next(21);
    parent.produce(11, parent.anchors_for({&a}), {});
    const ChildRecord at2 = parent.reg().child(42)->record;
    a.next(22);

    auto close = [&](std::uint64_t height, const Hash& hash) {
      return parent.sign(make_close_op(0, 0, CloseChild{42, height, hash}), {{0, &r0}});
    };
    CHECK(contains(op_error(parent, 3, close(1, a.archive.by_height(1).block.header.hash())), "recorded tip"));
    CHECK(contains(op_error(parent, 3, close(3, a.engine->tip().hash())), "recorded tip"));
    CHECK(contains(op_error(parent, 3, close(2, fill(0x09))), "recorded tip"));
    CHECK(contains(op_error(parent, 3, parent.sign(make_close_op(0, 0, CloseChild{99, 0, fill(0)}), {{0, &r0}})),
                   "unregistered"));
    const SubchainOp close_op = close(at2.tip_height, at2.tip_hash);
    CHECK(contains(parent.refuses(12, parent.anchors_for({&a}), {close_op}), "closed child"));
    parent.produce(12, {}, {close_op});
    const ChildRecord closed = parent.reg().child(42)->record;
    CHECK(closed.status == ChildRecord::kClosed && closed.closed_height == 3 && closed.tip_height == 2);
    CHECK(closed.tip_hash == at2.tip_hash && closed.vset_hash == at2.vset_hash);
    CHECK(parent.reg().open_count() == 1 && parent.reg().record_count() == 2);
    CHECK(parent.reg().child(42)->vset == nullptr);
    CHECK(parent.reg().root() == naive_root(parent.reg()));

    CHECK(contains(op_error(parent, 4, close_op), "already closed"));
    Anchor late;
    late.child_chain_id = 42;
    late.headers = {CertifiedHeader{a.archive.by_height(3).block.header, a.archive.by_height(3).commit.encode()}};
    CHECK(contains(parent.refuses(13, {late}, {}), "closed child"));
    CHECK(!node::pending_anchor(parent.reg(), 42, a.archive).has_value());
    CHECK(contains(op_error(parent, 4, parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}})),
                   "already registered"));

    const std::optional<ChildRecord> proven = verify_child_proof(parent.tip().subchain_root, parent.reg().prove(42));
    CHECK(proven.has_value() && proven->status == ChildRecord::kClosed && proven->tip_hash == at2.tip_hash);
    b.next(30);
    parent.produce(13, parent.anchors_for({&b}), {});
    CHECK(verify_child_proof(parent.tip().subchain_root, parent.reg().prove(42)).has_value());

    Child c(44, 0x12);
    const ChildRegistration reg_c = c.registration();
    parent.produce(14, {},
                   {parent.sign(make_register_op(0, 0, reg_c), {{0, &r0}}),
                    parent.sign(make_close_op(0, 0, CloseChild{44, 0, reg_c.genesis_header.hash()}), {{0, &r0}})});
    CHECK(parent.reg().child(44)->record.status == ChildRecord::kClosed &&
          parent.reg().child(44)->record.registered_height == 5 && parent.reg().child(44)->record.closed_height == 5);
    CHECK(parent.reg().root() == naive_root(parent.reg()));
  }

  std::printf("== I16 registrar rotation: the epoch voids outstanding signatures ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    Child a(42, 0x10), b(43, 0x11), c(44, 0x12);
    const SubchainOp held = parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}});
    RegistrarSet two;
    two.threshold = 2;
    two.registrars = {ed_registrar(r1), ed_registrar(r2)};
    const SubchainOp rotate = parent.sign(make_set_registrars_op(0, 0, two), {{0, &r0}});
    const Hash before = parent.reg().root();
    parent.produce(10, {}, {rotate});
    CHECK(parent.reg().authority_epoch() == 1 && parent.reg().authority() == two);
    CHECK(parent.reg().root() != before && parent.reg().root() == naive_root(parent.reg()));
    CHECK(contains(op_error(parent, 2, held), "epoch"));
    SubchainOp old_key = make_register_op(1, 0, a.registration());
    approve_op(old_key, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    approve_op(old_key, 1, parent.genesis_hash, 1, SigAlgo::kEd25519, v(r2.secret_key));
    CHECK(contains(op_error(parent, 2, old_key), "bad registrar signature"));
    SubchainOp fresh = make_register_op(1, 0, a.registration());
    approve_op(fresh, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r1.secret_key));
    CHECK(contains(op_error(parent, 2, fresh), "not enough approvals"));
    approve_op(fresh, 1, parent.genesis_hash, 1, SigAlgo::kEd25519, v(r2.secret_key));
    parent.produce(11, {}, {fresh});
    CHECK(parent.reg().child(42) != nullptr);

    RegistrarSet one;
    one.threshold = 1;
    one.registrars = {ed_registrar(r0)};
    SubchainOp back = make_set_registrars_op(1, 0, one);
    approve_op(back, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r1.secret_key));
    approve_op(back, 1, parent.genesis_hash, 1, SigAlgo::kEd25519, v(r2.secret_key));
    SubchainOp same_block = make_register_op(2, 0, b.registration());
    approve_op(same_block, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    SubchainOp stale_in_block = make_register_op(1, 0, c.registration());
    approve_op(stale_in_block, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r1.secret_key));
    approve_op(stale_in_block, 1, parent.genesis_hash, 1, SigAlgo::kEd25519, v(r2.secret_key));
    CHECK(contains(parent.refuses(12, {}, {back, stale_in_block}), "epoch"));
    parent.produce(12, {}, {back, same_block});
    CHECK(parent.reg().authority_epoch() == 2 && parent.reg().child(43) != nullptr);
    CHECK_MSG(contains(op_error(parent, 4, rotate), "epoch"), "an old rotation cannot be replayed after rotating back");
    CHECK(contains(op_error(parent, 4, held), "epoch"));

    RegistrarSet invalid = one;
    invalid.threshold = 2;
    SubchainOp bad = make_set_registrars_op(2, 0, invalid);
    approve_op(bad, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    CHECK(contains(op_error(parent, 4, bad), "threshold"));

    SubchainOp seal = make_set_registrars_op(2, 0, RegistrarSet{});
    approve_op(seal, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    parent.produce(13, {}, {seal});
    CHECK(parent.reg().authority_epoch() == 3 && parent.reg().authority().registrars.empty());
    SubchainOp after = make_register_op(3, 0, c.registration());
    approve_op(after, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    CHECK(contains(op_error(parent, 5, after), "sealed"));
    a.next(20);
    parent.produce(14, parent.anchors_for({&a}), {});
    CHECK_MSG(parent.reg().child(42)->record.tip_height == 1, "a sealed registry still anchors");
    CHECK(verify_child_proof(parent.tip().subchain_root, parent.reg().prove(42)).has_value());
  }

  std::printf("== a registry sealed at genesis is the static case with Merkle proofs ==\n");
  {
    Child a(42, 0x10), b(43, 0x11);
    DynamicSubchainPolicy p;
    p.max_depth = 1;
    p.children = {a.registration(), b.registration()};
    Parent parent(1, p);
    SubchainOp op = make_register_op(0, 0, Child(44, 0x12).registration());
    approve_op(op, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    CHECK(contains(op_error(parent, 1, op), "sealed"));
    a.next(20);
    b.next(21);
    parent.produce(10, parent.anchors_for({&a, &b}), {});
    for (std::uint64_t id : {std::uint64_t{42}, std::uint64_t{43}}) {
      const auto rec = verify_child_proof(parent.tip().subchain_root, parent.reg().prove(id));
      CHECK(rec.has_value() && rec->tip_height == 1 && rec->registered_height == 0);
    }
  }

  std::printf("== post quantum registrars: hybrid and SLH-DSA keys authorise operations ==\n");
  {
    const crypto::KeyPair hybrid = crypto::hybrid_sig_keygen();
    const crypto::KeyPair slh = crypto::slh_dsa_keygen();
    DynamicSubchainPolicy p;
    p.max_depth = 1;
    p.authority.threshold = 2;
    p.authority.registrars = {Registrar{2, hybrid.public_key}, Registrar{4, slh.public_key}};
    CHECK(p.validate(1).empty());
    Parent parent(1, p);
    Child a(42, 0x10);
    SubchainOp op = make_register_op(0, 0, a.registration());
    approve_op(op, 1, parent.genesis_hash, 0, SigAlgo::kHybridEd25519MlDsa, v(hybrid.secret_key));
    CHECK(contains(op_error(parent, 1, op), "not enough approvals"));
    approve_op(op, 1, parent.genesis_hash, 1, SigAlgo::kSlhDsa, v(slh.secret_key));
    parent.produce(10, {}, {op});
    CHECK(parent.reg().child(42) != nullptr);
    SubchainOp wrong_scheme = make_register_op(0, 0, Child(43, 0x11).registration());
    approve_op(wrong_scheme, 1, parent.genesis_hash, 0, SigAlgo::kEd25519, v(r0.secret_key));
    approve_op(wrong_scheme, 1, parent.genesis_hash, 1, SigAlgo::kSlhDsa, v(slh.secret_key));
    CHECK(contains(op_error(parent, 2, wrong_scheme), "bad registrar signature"));
    CHECK_THROWS(approve_op(wrong_scheme, 1, parent.genesis_hash, 5, SigAlgo::kUtxoWitness, v(r0.secret_key)));
  }

  std::printf("== I19 the incremental root equals the naive root under random load ==\n");
  {
    DynamicSubchainPolicy p = base_policy({&r0}, 1);
    Parent parent(1, p);
    std::vector<std::unique_ptr<Child>> kids;
    std::uint64_t seed = 0x1234567ull;
    auto rnd = [&](std::uint64_t n) {
      seed ^= seed << 13;
      seed ^= seed >> 7;
      seed ^= seed << 17;
      return seed % n;
    };
    bool roots_agree = true, proofs_hold = true, snapshots_agree = true;
    std::vector<bool> closed;
    for (std::uint64_t round = 0; round < 40; ++round) {
      std::vector<SubchainOp> ops;
      const std::uint64_t fresh = rnd(4);
      for (std::uint64_t i = 0; i < fresh; ++i) {
        kids.push_back(std::make_unique<Child>(1000 + kids.size(), static_cast<std::uint8_t>(kids.size() + 1)));
        closed.push_back(false);
        ops.push_back(parent.sign(make_register_op(0, 0, kids.back()->registration()), {{0, &r0}}));
      }
      std::vector<const Child*> advancing;
      const std::size_t known = kids.size() - fresh;
      for (std::size_t i = 0; i < known; ++i) {
        if (closed[i]) continue;
        const std::uint64_t what = rnd(6);
        if (what < 3) {
          const std::uint64_t blocks = 1 + rnd(3);
          for (std::uint64_t k = 0; k < blocks; ++k) kids[i]->next(100 + round);
          advancing.push_back(kids[i].get());
        }
        if (what == 5) {
          const ChildRecord rec = parent.reg().child(kids[i]->genesis.chain_id)->record;
          ops.push_back(parent.sign(make_close_op(0, 0, CloseChild{rec.chain_id, rec.tip_height, rec.tip_hash}),
                                    {{0, &r0}}));
          closed[i] = true;
        }
      }
      DynamicSubchainRegistry working = parent.reg();
      const std::vector<Anchor> anchors = parent.anchors_for(advancing);
      roots_agree &= working.apply(round + 1, ops, anchors).empty();
      roots_agree &= working.root() == naive_root(working);
      for (std::size_t i = 0; i < kids.size() && i < working.record_count(); i += 1 + rnd(3)) {
        const ChildProof proof = working.prove(kids[i]->genesis.chain_id);
        proofs_hold &= verify_child_proof(working.root(), proof).has_value();
      }
      const Hash uncompacted = working.root();
      const Bytes uncompacted_bytes = working.serialize();
      working.compact();
      roots_agree &= working.root() == uncompacted;
      snapshots_agree &= working.serialize() == uncompacted_bytes;
      parent.produce(10 + round, anchors, ops);
      roots_agree &= parent.reg().root() == uncompacted && parent.reg().root() == naive_root(parent.reg());
      roots_agree &= parent.follower->dynamic_subchains()->root() == parent.reg().root();
      DynamicSubchainRegistry restored = parent.reg();
      restored.restore(v(parent.reg().serialize()));
      snapshots_agree &= restored.root() == parent.reg().root() && restored.serialize() == parent.reg().serialize();
    }
    CHECK_MSG(roots_agree, "overlay root, compacted root, follower root and naive root agree for 40 random blocks");
    CHECK_MSG(proofs_hold, "proofs taken from an uncommitted working copy verify");
    CHECK_MSG(snapshots_agree, "serialize is the same before and after compaction and round trips");
    CHECK(kids.size() > 20 && parent.reg().open_count() < parent.reg().record_count());
  }

  std::printf("== child validator rotation is followed through anchors and snapshots ==\n");
  {
    Parent parent(1, base_policy({&r0}, 1));
    Child a(42, 0x10);
    parent.produce(10, {}, {parent.sign(make_register_op(0, 0, a.registration()), {{0, &r0}})});
    const Hash genesis_set = parent.reg().child(42)->record.vset_hash;
    const crypto::KeyPair extra = bls(0x1F);
    std::vector<consensus::Validator> next = a.engine->validators().validators();
    next.push_back(consensus::Validator{extra.public_key, 1});
    a.engine->schedule_validator_set(consensus::ValidatorSet(next));
    a.next(20);
    parent.produce(11, parent.anchors_for({&a}), {});
    const ChildRecord announced = parent.reg().child(42)->record;
    CHECK(announced.tip_vset_hash == genesis_set && announced.vset_hash == consensus::ValidatorSet(next).hash());
    auto restored = Ledger::restore(parent.genesis.chain_spec(), v(parent.engine->ledger().snapshot()));
    CHECK(restored->dynamic_subchains()->root() == parent.reg().root());
    CHECK(restored->dynamic_subchains()->child(42)->vset->hash() == announced.vset_hash);
    Block b2 = a.engine->propose({}, 21);
    auto cert = a.engine->try_commit(b2, {a.engine->precommit(b2, 0, a.key), a.engine->precommit(b2, 1, extra)});
    CHECK(cert.has_value() && a.engine->finalize(b2, *cert).ok && a.archive.put(b2, *cert));
    parent.produce(12, parent.anchors_for({&a}), {});
    CHECK(parent.reg().child(42)->record.tip_height == 2 &&
          parent.reg().child(42)->record.tip_vset_hash == announced.vset_hash);
    CHECK(parent.reg().root() == naive_root(parent.reg()));
  }

  std::printf("== I21 persistence: ledger snapshot, chain snapshot, strict restore ==\n");
  {
    const DynamicSubchainPolicy p = base_policy({&r0}, 1);
    auto script = [&](Parent& parent, std::vector<std::unique_ptr<Child>>& kids, std::uint64_t step) {
      if (step == 0) {
        for (std::uint64_t i = 0; i < 3; ++i) kids.push_back(std::make_unique<Child>(42 + i, static_cast<std::uint8_t>(0x10 + i)));
        return parent.produce(10, {},
                              {parent.sign(make_register_op(0, 0, kids[0]->registration()), {{0, &r0}}),
                               parent.sign(make_register_op(0, 0, kids[1]->registration()), {{0, &r0}})});
      }
      if (step == 1) {
        kids[0]->next(20);
        kids[1]->next(20);
        return parent.produce(11, parent.anchors_for({kids[0].get(), kids[1].get()}), {});
      }
      if (step == 2) {
        const ChildRecord rec = parent.reg().child(43)->record;
        return parent.produce(12, {},
                              {parent.sign(make_close_op(0, 0, CloseChild{43, rec.tip_height, rec.tip_hash}),
                                           {{0, &r0}}),
                               parent.sign(make_register_op(0, 0, kids[2]->registration()), {{0, &r0}})});
      }
      kids[0]->next(21);
      kids[2]->next(21);
      return parent.produce(10 + step, parent.anchors_for({kids[0].get(), kids[2].get()}), {});
    };
    Parent straight(1, p);
    std::vector<std::unique_ptr<Child>> straight_kids;
    std::vector<Bytes> expected;
    for (std::uint64_t s = 0; s < 5; ++s) expected.push_back(script(straight, straight_kids, s).encode());

    Parent first(1, p);
    std::vector<std::unique_ptr<Child>> kids;
    std::vector<Bytes> got;
    for (std::uint64_t s = 0; s < 3; ++s) got.push_back(script(first, kids, s).encode());
    const Bytes ledger_snapshot = first.engine->ledger().snapshot();
    auto restored = Ledger::restore(first.genesis.chain_spec(), v(ledger_snapshot));
    CHECK(restored->dynamic_subchains()->root() == first.reg().root() && restored->snapshot() == ledger_snapshot);
    CHECK(restored->dynamic_subchains()->child(43)->record.status == ChildRecord::kClosed);

    codec::CodecReader lr(v(ledger_snapshot));
    const Bytes tip_bytes = lr.read_bytes();
    const Bytes state_bytes = lr.read_bytes();
    const Bytes registry_bytes = lr.read_bytes();
    auto with_registry = [&](const Bytes& registry) {
      codec::CodecWriter w;
      w.write_bytes(v(tip_bytes));
      w.write_bytes(v(state_bytes));
      w.write_bytes(v(registry));
      return w.take();
    };
    CHECK(Ledger::restore(first.genesis.chain_spec(), v(with_registry(registry_bytes)))->snapshot() == ledger_snapshot);
    Bytes trailing = registry_bytes;
    trailing.push_back(0);
    CHECK_THROWS(Ledger::restore(first.genesis.chain_spec(), v(with_registry(trailing))));
    CHECK_THROWS(Ledger::restore(first.genesis.chain_spec(),
                                 v(with_registry(Bytes(registry_bytes.begin(), registry_bytes.end() - 1)))));
    bool every_flip_rejected = true;
    for (std::size_t i = 0; i < registry_bytes.size(); ++i) {
      Bytes flipped = registry_bytes;
      flipped[i] ^= 0x01;
      try {
        (void)Ledger::restore(first.genesis.chain_spec(), v(with_registry(flipped)));
        every_flip_rejected = false;
      } catch (const std::exception&) {
      }
    }
    CHECK_MSG(every_flip_rejected, "every single bit flip in the registry section is refused");
    CHECK_THROWS(Ledger::restore(first.genesis.chain_spec(), v(with_registry(Bytes{}))));

    DynamicSubchainRegistry carried = *restored->dynamic_subchains();
    kids[0]->next(21);
    kids[2]->next(21);
    std::vector<Anchor> anchors;
    for (std::uint64_t id : {std::uint64_t{42}, std::uint64_t{44}})
      anchors.push_back(*node::pending_anchor(carried, id, kids[id - 42]->archive));
    Block resumed = restored->build_block({}, 13, anchors, {});
    consensus::ConsensusData cd{first.engine->validators().hash(), first.engine->last_commit()};
    resumed.header.consensus_data = cd.encode();
    CHECK_MSG(resumed.encode() == expected[3], "a restored ledger builds the block the uninterrupted run built");
    CHECK(restored->process_block(resumed).ok);
    CHECK(restored->dynamic_subchains()->root() == Block::decode_all(v(expected[3])).header.subchain_root);
    CHECK(got == std::vector<Bytes>(expected.begin(), expected.begin() + 3));
  }

  std::printf("== I21 an assembled dynamic parent restarts from a chain snapshot ==\n");
  {
    GenesisSpec g;
    g.chain_id = 7;
    g.timestamp = 1;
    const crypto::KeyPair k = bls(0x71);
    g.validators = {consensus::Validator{k.public_key, 1}};
    AssuranceProfile::standard().apply_to(g);
    ChainBlueprint{"bft", "account", "", {}}.commit_to(g);
    base_policy({&r0}, 1).commit_to(g);
    const Hash gh = g.hash();
    auto run = [&](std::uint64_t restart_after) {
      node::AssembledChain chain = node::ChainAssembler::assemble(g, 0, k);
      BlockStore archive;
      Child a(42, 0x10), b(43, 0x11);
      std::vector<Bytes> blocks;
      auto step = [&](std::uint64_t ts, std::vector<Anchor> anchors, std::vector<SubchainOp> ops) {
        auto& bft = dynamic_cast<consensus::BftEngine&>(*chain.engine);
        Block blk = chain.engine->propose_subchain({}, ts, std::move(anchors), std::move(ops));
        auto cert = bft.try_commit(blk, {bft.precommit(blk, 0, k)});
        const Bytes just = cert->encode();
        CHECK(chain.engine->submit(blk, v(just)).ok && archive.put(blk, *cert));
        blocks.push_back(blk.encode());
        if (blocks.size() != restart_after) return;
        const Bytes snap = node::snapshot_chain(gh, chain, &archive);
        node::RestoredChain back = node::restore_chain(g, 0, k, v(snap));
        CHECK(node::snapshot_chain(gh, back.chain, &*back.archive) == snap);
        chain = std::move(back.chain);
        archive = std::move(*back.archive);
      };
      auto signed_op = [&](SubchainOp op) {
        approve_op(op, 7, gh, 0, SigAlgo::kEd25519, v(r0.secret_key));
        return op;
      };
      auto anchor = [&](const Child& c) {
        return std::vector<Anchor>{*node::pending_anchor(*chain.engine->ledger().dynamic_subchains(),
                                                         c.genesis.chain_id, c.archive)};
      };
      step(10, {}, {signed_op(make_register_op(0, 0, a.registration()))});
      a.next(20);
      step(11, anchor(a), {signed_op(make_register_op(0, 0, b.registration()))});
      const ChildRecord rec = chain.engine->ledger().dynamic_subchains()->child(42)->record;
      b.next(21);
      step(12, anchor(b), {signed_op(make_close_op(0, 0, CloseChild{42, rec.tip_height, rec.tip_hash}))});
      b.next(22);
      step(13, anchor(b), {});
      blocks.push_back(chain.engine->ledger().dynamic_subchains()->prove(43).encode());
      blocks.push_back(node::snapshot_chain(gh, chain, &archive));
      return blocks;
    };
    const std::vector<Bytes> straight = run(0);
    CHECK(run(1) == straight);
    CHECK(run(2) == straight);
    CHECK(run(3) == straight);
    CHECK(run(4) == straight);
  }

  std::printf("== mixed proofs: version 4 over version 3 and version 3 over version 4 ==\n");
  {
    const Hash alice = fill(0xA1);
    {
      Child leaf(420, 0x40, &alice);
      SubchainPolicy flat;
      flat.max_children = 1;
      flat.max_depth = 1;
      flat.children = {leaf.registration()};
      GenesisSpec mid_genesis;
      mid_genesis.chain_id = 42;
      mid_genesis.timestamp = 1;
      const crypto::KeyPair mid_key = bls(0x41);
      mid_genesis.validators = {consensus::Validator{mid_key.public_key, 1}};
      flat.commit_to(mid_genesis);
      Child mid(mid_genesis, mid_key);
      Parent root(1, base_policy({&r0}, 1));
      root.produce(10, {}, {root.sign(make_register_op(0, 0, mid.registration(1)), {{0, &r0}})});
      leaf.next(20);
      mid.next(30, {*node::pending_anchor(*mid.engine->ledger().subchains(), 420, leaf.archive)});
      root.produce(11, root.anchors_for({&mid}), {});

      SubchainProofV2 proof;
      proof.hops.push_back({SubchainProofV2::kRecordHop, root.reg().prove(42).encode()});
      proof.hops.push_back({SubchainProofV2::kFlatHop, mid.engine->ledger().subchains()->hop(420).encode()});
      proof.state_proof =
          dynamic_cast<const AccountStateMachine&>(leaf.engine->ledger().state()).prove_account(alice);
      CHECK(verify_subchain_proof_v2(root.tip().subchain_root, proof));
      CHECK(SubchainProofV2::decode_all(v(proof.encode())).encode() == proof.encode());
      SubchainProofV2 bad = proof;
      std::swap(bad.hops[0], bad.hops[1]);
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.hops[0].kind = SubchainProofV2::kFlatHop;
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.hops[1].kind = SubchainProofV2::kRecordHop;
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.hops.pop_back();
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.hops.clear();
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.hops[1].body[bad.hops[1].body.size() / 2] ^= 1;
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      bad = proof;
      bad.state_proof.leaf_preimage[40] ^= 1;
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
      CHECK(!verify_subchain_proof_v2(fill(0x01), proof));
      Bytes unknown_kind = proof.encode();
      unknown_kind[4] = 3;
      CHECK_THROWS(SubchainProofV2::decode_all(v(unknown_kind)));
      bad = proof;
      bad.hops[0].kind = 3;
      CHECK(!verify_subchain_proof_v2(root.tip().subchain_root, bad));
    }
    {
      Child leaf(421, 0x50, &alice);
      DynamicSubchainPolicy mid_policy = base_policy({&r1}, 1);
      mid_policy.max_depth = 1;
      Parent mid(43, mid_policy, 1, 0x51);
      mid.produce(10, {}, {mid.sign(make_register_op(0, 0, leaf.registration()), {{0, &r1}})});
      leaf.next(20);
      mid.produce(11, mid.anchors_for({&leaf}), {});

      ChildRegistration mid_reg;
      mid_reg.chain_id = 43;
      mid_reg.genesis_header = ledger_from_genesis(mid.genesis)->tip();
      mid_reg.validators = mid.genesis.validators;
      mid_reg.child_max_depth = 1;
      SubchainPolicy flat;
      flat.max_children = 1;
      flat.max_depth = 2;
      flat.children = {mid_reg};
      GenesisSpec root_genesis;
      root_genesis.chain_id = 2;
      root_genesis.timestamp = 1;
      flat.commit_to(root_genesis);
      auto root = ledger_from_genesis(root_genesis);
      Block rb = root->build_block({}, 30, {*node::pending_anchor(*root->subchains(), 43, mid.archive)});
      CHECK(root->process_block(rb).ok && rb.header.version == 3);

      SubchainProofV2 proof;
      proof.hops.push_back({SubchainProofV2::kFlatHop, root->subchains()->hop(43).encode()});
      proof.hops.push_back({SubchainProofV2::kRecordHop, mid.reg().prove(421).encode()});
      proof.state_proof =
          dynamic_cast<const AccountStateMachine&>(leaf.engine->ledger().state()).prove_account(alice);
      CHECK(verify_subchain_proof_v2(root->tip().subchain_root, proof));
      SubchainProofV2 bad = proof;
      bad.hops[1].body[20] ^= 1;
      CHECK(!verify_subchain_proof_v2(root->tip().subchain_root, bad));

      SubchainProof v1_proof;
      v1_proof.hops = {root->subchains()->hop(43)};
      v1_proof.state_proof = proof.state_proof;
      CHECK_MSG(!verify_subchain_proof(root->tip().subchain_root, v1_proof),
                "the version 1 verifier cannot be pointed at a dynamic child's state through a flat hop alone");
    }
  }

  std::printf("== every built in engine can propose operations ==\n");
  {
    const DynamicSubchainPolicy p = base_policy({&r0}, 1);
    GenesisSpec g;
    g.chain_id = 6;
    g.timestamp = 1;
    const crypto::KeyPair k = bls(0x61);
    g.validators = {consensus::Validator{k.public_key, 1}};
    p.commit_to(g);
    consensus::PosEngine pos(g.validator_set(), 0, k, ledger_from_genesis(g));
    Child a(42, 0x10);
    SubchainOp op = make_register_op(0, 0, a.registration());
    approve_op(op, 6, g.hash(), 0, SigAlgo::kEd25519, v(r0.secret_key));
    Block b = pos.propose_subchain({}, 10, {}, {op});
    CHECK(b.header.version == 4 && b.ops.size() == 1);
    CHECK(pos.submit(b, v(pos.seal(b))).ok);
    CHECK(pos.ledger().dynamic_subchains()->child(42) != nullptr);
    CHECK(pos.propose({}, 11).header.version == 4);
  }

  return bctest::summary();
}
