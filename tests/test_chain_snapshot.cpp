#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/assurance.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/chain_snapshot.hpp"
#include "aviary/chain_tree.hpp"
#include "aviary/genesis.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/pow_engine.hpp"
#include "aviary/subchain.hpp"
#include "aviary/subchain_relay.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using node::AssembledChain;
using node::ChainAssembler;
using node::ChainSnapshot;
using node::ChainTree;
using node::ChainTreeSpec;

namespace {
codec::ByteView v(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Hash fill(std::uint8_t x) {
  Hash h;
  h.fill(x);
  return h;
}
crypto::KeyPair bls(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return crypto::bls_keygen_from_ikm(v(ikm));
}
crypto::KeyPair ed(std::uint8_t seed) {
  Bytes s(32, seed);
  return crypto::ed25519_keygen_from_seed(v(s));
}

Transaction transfer(std::uint64_t chain, const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce,
                     codec::u128 amount) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, from);
  return tx;
}

struct Family {
  GenesisSpec genesis;
  std::vector<crypto::KeyPair> keys;
};

Family bft_family(std::uint64_t chain_id, std::uint8_t seed, std::size_t validators, const Hash& funded) {
  Family f;
  f.genesis.chain_id = chain_id;
  f.genesis.timestamp = 1;
  f.genesis.accounts = {GenesisAccount{funded, 1000000}};
  for (std::size_t i = 0; i < validators; ++i) {
    f.keys.push_back(bls(static_cast<std::uint8_t>(seed + i)));
    f.genesis.validators.push_back(consensus::Validator{f.keys.back().public_key, 1});
  }
  AssuranceProfile::standard().apply_to(f.genesis);
  ChainBlueprint{"bft", "account", "", {}}.commit_to(f.genesis);
  return f;
}

struct Running {
  Family family;
  AssembledChain chain;
  BlockStore archive;

  explicit Running(Family f)
      : family(std::move(f)), chain(ChainAssembler::assemble(family.genesis, 0, family.keys[0])) {}
  Running(const Family& f, const Bytes& snapshot)
      : Running(f, node::restore_chain(f.genesis, 0, f.keys[0], v(snapshot))) {}
  Running(const Family& f, node::RestoredChain r)
      : family(f), chain(std::move(r.chain)), archive(std::move(r.archive.value())) {}
  consensus::BftEngine& bft() { return dynamic_cast<consensus::BftEngine&>(*chain.engine); }

  Bytes step(std::uint64_t ts, std::vector<Transaction> txs = {}, std::vector<Anchor> anchors = {}) {
    Block b = chain.engine->propose_anchored(std::move(txs), ts, std::move(anchors));
    std::vector<consensus::Vote> votes;
    for (const crypto::KeyPair& k : family.keys)
      if (auto idx = bft().validators().index_of(k.public_key)) votes.push_back(bft().precommit(b, *idx, k));
    auto cert = bft().try_commit(b, votes);
    if (!cert) throw std::runtime_error("no quorum");
    const Bytes just = cert->encode();
    if (BlockError e = chain.engine->submit(b, v(just)); !e.ok) throw std::runtime_error(e.error);
    archive.put(b, *cert);
    return b.encode();
  }
  Bytes snapshot() const { return node::snapshot_chain(family.genesis.hash(), chain, &archive); }
};

bool restore_throws(const Family& f, const Bytes& snapshot) {
  try {
    (void)node::restore_chain(f.genesis, 0, f.keys[0], v(snapshot));
    return false;
  } catch (const std::exception&) {
    return true;
  }
}

ChainTreeSpec::Validator tree_validator(std::uint8_t seed) {
  crypto::KeyPair k = bls(seed);
  return ChainTreeSpec::Validator{k.public_key, k.secret_key, 1};
}
}  // namespace

int main() {
  init();
  const crypto::KeyPair alice = ed(1);
  const Hash alice_addr = crypto::address_of(v(alice.public_key));
  const Hash bob = fill(0xB0);

  std::printf("== restart: snapshot, destroy, restore, continue equals an uninterrupted run ==\n");
  Bytes reference_snapshot;
  {
    Running straight(bft_family(7, 0x10, 3, alice_addr));
    std::vector<Bytes> expected;
    for (std::uint64_t i = 0; i < 6; ++i)
      expected.push_back(straight.step(10 + i, {transfer(7, alice, bob, i, 5 + i)}));

    Bytes mid;
    std::vector<Bytes> got;
    {
      Running first(bft_family(7, 0x10, 3, alice_addr));
      for (std::uint64_t i = 0; i < 3; ++i) got.push_back(first.step(10 + i, {transfer(7, alice, bob, i, 5 + i)}));
      mid = first.snapshot();
    }
    Running second(bft_family(7, 0x10, 3, alice_addr), mid);
    CHECK(second.chain.engine->height() == 3);
    CHECK(second.snapshot() == mid);
    for (std::uint64_t i = 3; i < 6; ++i) got.push_back(second.step(10 + i, {transfer(7, alice, bob, i, 5 + i)}));
    CHECK(got == expected);
    CHECK(second.chain.engine->tip() == straight.chain.engine->tip());
    CHECK(second.chain.engine->ledger().state().state_root() == straight.chain.engine->ledger().state().state_root());
    CHECK(second.snapshot() == straight.snapshot());
    CHECK(second.archive.size() == 6 && second.archive.serialize() == straight.archive.serialize());
    reference_snapshot = mid;
  }

  std::printf("== a snapshot holds no secret key ==\n");
  {
    const Family f = bft_family(7, 0x10, 3, alice_addr);
    bool leaked = false;
    for (const crypto::KeyPair& k : f.keys) {
      const Bytes& s = k.secret_key;
      for (std::size_t i = 0; i + s.size() <= reference_snapshot.size() && !leaked; ++i)
        leaked = std::equal(s.begin(), s.end(), reference_snapshot.begin() + static_cast<std::ptrdiff_t>(i));
    }
    CHECK(!leaked);
  }

  std::printf("== a genesis snapshot restores to a chain that has produced nothing ==\n");
  {
    Running fresh(bft_family(7, 0x10, 3, alice_addr));
    const Bytes snap = fresh.snapshot();
    Running back(bft_family(7, 0x10, 3, alice_addr), snap);
    CHECK(back.chain.engine->height() == 0 && back.snapshot() == snap);
    CHECK(back.step(10) == fresh.step(10));
  }

  std::printf("== validator rotation: scheduled, announced and in force all survive a restart ==\n");
  {
    auto run = [&](int restart_after) {
      Family f = bft_family(8, 0x20, 3, alice_addr);
      const crypto::KeyPair extra = bls(0x2F);
      auto current = std::make_unique<Running>(f);
      std::vector<Bytes> blocks;
      auto maybe_restart = [&](int at) {
        if (at != restart_after) return;
        const Bytes snap = current->snapshot();
        Family again = current->family;
        current.reset();
        current = std::make_unique<Running>(again, snap);
        CHECK(current->snapshot() == snap);
      };
      blocks.push_back(current->step(10));
      std::vector<consensus::Validator> next = current->bft().validators().validators();
      next.push_back(consensus::Validator{extra.public_key, 1});
      current->bft().schedule_validator_set(consensus::ValidatorSet(next));
      current->family.keys.push_back(extra);
      maybe_restart(1);
      blocks.push_back(current->step(11));
      maybe_restart(2);
      blocks.push_back(current->step(12));
      maybe_restart(3);
      blocks.push_back(current->step(13));
      CHECK(current->bft().validators().size() == 4);
      return blocks;
    };
    const std::vector<Bytes> straight = run(0);
    CHECK(run(1) == straight);
    CHECK(run(2) == straight);
    CHECK(run(3) == straight);
  }

  std::printf("== recorded equivocators survive a restart ==\n");
  {
    Running r(bft_family(9, 0x30, 3, alice_addr));
    r.step(10);
    consensus::Evidence ev;
    ev.validator_index = 1;
    ev.height = 2;
    ev.round = 0;
    ev.block_id_a = fill(0x01);
    ev.block_id_b = fill(0x02);
    ev.signature_a = consensus::sign_vote(9, 2, 0, ev.block_id_a, 1, r.family.keys[1]).signature;
    ev.signature_b = consensus::sign_vote(9, 2, 0, ev.block_id_b, 1, r.family.keys[1]).signature;
    CHECK(r.bft().record_evidence(ev));
    const Bytes snap = r.snapshot();
    Running back(r.family, snap);
    CHECK(back.bft().equivocators() == r.bft().equivocators() && back.bft().equivocators().count(1) == 1);
    CHECK(back.bft().slashed_validator_set().hash() == r.bft().slashed_validator_set().hash());
  }

  std::printf("== a parent chain: registry, receipts and proofs survive a restart ==\n");
  {
    Family child = bft_family(42, 0x40, 3, alice_addr);
    Family parent = bft_family(1, 0x50, 1, alice_addr);
    SubchainPolicy policy;
    policy.max_children = 4;
    policy.max_depth = 1;
    ChildRegistration reg;
    reg.chain_id = 42;
    reg.genesis_header = ledger_from_genesis(child.genesis)->tip();
    reg.validators = child.genesis.validators;
    policy.children = {reg};
    policy.commit_to(parent.genesis);
    ParentBinding{1, parent.genesis.hash()}.commit_to(child.genesis);

    Running c(child);
    Running straight(parent);
    auto anchor_next = [&](Running& p) {
      auto a = node::pending_anchor(*p.chain.engine->ledger().subchains(), 42, c.archive);
      return a ? std::vector<Anchor>{*a} : std::vector<Anchor>{};
    };
    c.step(10);
    c.step(11);
    std::vector<Bytes> expected, got;
    expected.push_back(straight.step(20, {transfer(1, alice, bob, 0, 3)}, anchor_next(straight)));
    Running first(parent);
    got.push_back(first.step(20, {transfer(1, alice, bob, 0, 3)}, anchor_next(first)));
    const Bytes snap = first.snapshot();
    Running second(parent, snap);
    CHECK(second.chain.engine->ledger().last_receipts().size() == 1);
    CHECK(second.chain.engine->ledger().subchains()->root() == straight.chain.engine->ledger().subchains()->root());
    CHECK(second.chain.engine->ledger().subchains()->tip_of(42)->height == 2);
    c.step(12);
    expected.push_back(straight.step(21, {}, anchor_next(straight)));
    got.push_back(second.step(21, {}, anchor_next(second)));
    CHECK(got == expected);
    CHECK(second.chain.engine->ledger().subchains()->hop(42).encode() ==
          straight.chain.engine->ledger().subchains()->hop(42).encode());
    CHECK(second.snapshot() == straight.snapshot());

    Bytes tampered = snap;
    ChainSnapshot parts = ChainSnapshot::decode_all(v(snap));
    parts.receipts[0].fee += 1;
    CHECK(restore_throws(parent, parts.encode()));
    parts = ChainSnapshot::decode_all(v(snap));
    parts.receipts.clear();
    CHECK(restore_throws(parent, parts.encode()));
  }

  std::printf("== other engines and state models round trip ==\n");
  {
    GenesisSpec pos;
    pos.chain_id = 11;
    pos.timestamp = 1;
    const crypto::KeyPair k = bls(0x60);
    pos.validators = {consensus::Validator{k.public_key, 1}};
    AssuranceProfile::standard().apply_to(pos);
    ChainBlueprint{"pos", "record", "round-robin", {}}.commit_to(pos);
    pos.state_model = StateModel::kRecord;
    AssembledChain a = ChainAssembler::assemble(pos, 0, k);
    auto advance = [&](AssembledChain& chain, std::uint64_t ts) {
      Block b = chain.engine->propose({}, ts);
      const Bytes seal = dynamic_cast<consensus::PosEngine&>(*chain.engine).seal(b);
      CHECK(chain.engine->submit(b, v(seal)).ok);
      return b.encode();
    };
    advance(a, 5);
    const Bytes snap = node::snapshot_chain(pos.hash(), a, nullptr);
    node::RestoredChain back = node::restore_chain(pos, 0, k, v(snap));
    CHECK(!back.archive.has_value());
    CHECK(node::snapshot_chain(pos.hash(), back.chain, nullptr) == snap);
    CHECK(advance(back.chain, 6) == advance(a, 6));

    GenesisSpec pow;
    pow.chain_id = 12;
    pow.timestamp = 1;
    pow.validators = {consensus::Validator{k.public_key, 1}};
    AssuranceProfile::standard().apply_to(pow);
    ChainBlueprint{"pow", "account", "", {}}.commit_to(pow);
    AssembledChain w = ChainAssembler::assemble(pow, 0, k);
    Hash harder;
    harder.fill(0x7F);
    dynamic_cast<consensus::PowEngine&>(*w.engine).set_target(harder);
    const Bytes wsnap = node::snapshot_chain(pow.hash(), w, nullptr);
    node::RestoredChain wback = node::restore_chain(pow, 0, k, v(wsnap));
    CHECK(dynamic_cast<consensus::PowEngine&>(*wback.chain.engine).target() == harder);
    ChainSnapshot parts = ChainSnapshot::decode_all(v(wsnap));
    parts.engine.pop_back();
    CHECK_THROWS(node::restore_chain(pow, 0, k, v(parts.encode())));
    parts = ChainSnapshot::decode_all(v(snap));
    parts.engine = Bytes{1};
    CHECK_THROWS(node::restore_chain(pos, 0, k, v(parts.encode())));
  }

  std::printf("== strict decoding and every restore rejection ==\n");
  {
    const Family f = bft_family(7, 0x10, 3, alice_addr);
    const Bytes& good = reference_snapshot;
    CHECK(!restore_throws(f, good));
    CHECK(ChainSnapshot::decode_all(v(good)).encode() == good);

    Bytes trailing = good;
    trailing.push_back(0);
    CHECK(restore_throws(f, trailing));
    CHECK(restore_throws(f, Bytes(good.begin(), good.end() - 1)));
    CHECK(restore_throws(f, Bytes{}));
    Bytes magic = good;
    magic[0] ^= 0xFF;
    CHECK(restore_throws(f, magic));
    Bytes version = good;
    version[4] = 2;
    CHECK(restore_throws(f, version));
    CHECK_THROWS(ChainSnapshot::decode_all(v(version)));

    Family other = bft_family(7, 0x10, 3, alice_addr);
    other.genesis.timestamp = 2;
    CHECK(restore_throws(other, good));

    ChainSnapshot parts = ChainSnapshot::decode_all(v(good));
    parts.genesis_hash = fill(0x99);
    CHECK(restore_throws(f, parts.encode()));

    parts = ChainSnapshot::decode_all(v(good));
    parts.ledger[parts.ledger.size() - 1] ^= 0x01;
    CHECK(restore_throws(f, parts.encode()));

    parts = ChainSnapshot::decode_all(v(good));
    parts.engine[parts.engine.size() / 2] ^= 0x01;
    CHECK(restore_throws(f, parts.encode()));

    parts = ChainSnapshot::decode_all(v(good));
    parts.engine.clear();
    CHECK(restore_throws(f, parts.encode()));

    Bytes flag = good;
    {
      ChainSnapshot none = ChainSnapshot::decode_all(v(good));
      none.archive.reset();
      flag = none.encode();
      CHECK(!restore_throws(f, flag));
      flag.back() = 2;
      CHECK(restore_throws(f, flag));
    }

    auto with_ledger = [&](const Bytes& snapshot, auto&& mutate) {
      ChainSnapshot p = ChainSnapshot::decode_all(v(snapshot));
      codec::CodecReader lr(v(p.ledger));
      Bytes tip = lr.read_bytes();
      Bytes state = lr.read_bytes();
      mutate(tip, state);
      codec::CodecWriter lw;
      lw.write_bytes(v(tip));
      lw.write_bytes(v(state));
      p.ledger = lw.take();
      return p.encode();
    };
    CHECK(restore_throws(f, with_ledger(good, [](Bytes&, Bytes& state) { state[15] ^= 0x01; })));
    {
      Running fresh(f);
      const Bytes genesis_snapshot = fresh.snapshot();
      CHECK(!restore_throws(f, genesis_snapshot));
      CHECK(restore_throws(f, with_ledger(genesis_snapshot, [](Bytes& tip, Bytes&) {
        BlockHeader h = BlockHeader::decode_all(v(tip));
        h.timestamp += 1;
        tip = h.encode();
      })));
    }
    {
      ChainSnapshot p = ChainSnapshot::decode_all(v(good));
      BlockStore stored = BlockStore::load(v(*p.archive));
      codec::CodecWriter aw;
      aw.write_u32(static_cast<std::uint32_t>(stored.size()));
      for (std::uint64_t h = stored.first_height(); h <= stored.tip_height(); ++h) {
        StoredBlock sb = stored.by_height(h);
        if (h == stored.tip_height()) sb.commit.round += 1;
        const Bytes blk = sb.block.encode();
        const Bytes cert = sb.commit.encode();
        aw.write_u64(h);
        aw.write_bytes(v(blk));
        aw.write_bytes(v(cert));
      }
      p.archive = aw.take();
      CHECK(restore_throws(f, p.encode()));
    }

    Running donor(bft_family(7, 0x10, 3, alice_addr));
    for (std::uint64_t i = 0; i < 2; ++i) donor.step(10 + i, {transfer(7, alice, bob, i, 5 + i)});
    parts = ChainSnapshot::decode_all(v(good));
    parts.archive = donor.archive.serialize();
    CHECK(restore_throws(f, parts.encode()));

    Running other_chain(bft_family(7, 0x10, 3, alice_addr));
    for (std::uint64_t i = 0; i < 3; ++i) other_chain.step(50 + i);
    parts = ChainSnapshot::decode_all(v(good));
    parts.archive = other_chain.archive.serialize();
    CHECK(restore_throws(f, parts.encode()));

    parts = ChainSnapshot::decode_all(v(good));
    BlockStore gap = BlockStore::load(v(*parts.archive));
    Bytes raw = gap.serialize();
    codec::CodecWriter w;
    w.write_u32(2);
    for (std::uint64_t h : {std::uint64_t{1}, std::uint64_t{3}}) {
      const StoredBlock& sb = gap.by_height(h);
      const Bytes blk = sb.block.encode();
      const Bytes cert = sb.commit.encode();
      w.write_u64(h);
      w.write_bytes(v(blk));
      w.write_bytes(v(cert));
    }
    parts.archive = w.take();
    CHECK(restore_throws(f, parts.encode()));

    codec::CodecWriter dup;
    dup.write_u32(4);
    for (std::uint64_t h : {std::uint64_t{1}, std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{3}}) {
      const StoredBlock& sb = gap.by_height(h);
      const Bytes blk = sb.block.encode();
      const Bytes cert = sb.commit.encode();
      dup.write_u64(h);
      dup.write_bytes(v(blk));
      dup.write_bytes(v(cert));
    }
    parts.archive = dup.take();
    CHECK(restore_throws(f, parts.encode()));
  }

  std::printf("== block store pruning keeps what remains contiguous and indexed ==\n");
  {
    Running r(bft_family(7, 0x10, 3, alice_addr));
    for (std::uint64_t i = 0; i < 5; ++i) r.step(10 + i, {transfer(7, alice, bob, i, 5 + i)});
    const Hash first_hash = r.archive.by_height(1).block.header.hash();
    const Hash first_tx = r.archive.by_height(1).block.transactions[0].txid();
    CHECK(r.archive.first_height() == 1 && r.archive.prune_below(4) == 3);
    CHECK(r.archive.first_height() == 4 && r.archive.tip_height() == 5 && r.archive.size() == 2);
    CHECK(!r.archive.has_hash(first_hash) && !r.archive.find_tx(first_tx).has_value());
    CHECK(r.archive.prune_below(4) == 0);
    const Bytes snap = r.snapshot();
    Running back(r.family, snap);
    CHECK(back.archive.first_height() == 4 && back.snapshot() == snap);
    CHECK(back.step(20) == r.step(20));
    CHECK(r.archive.prune_below(100) == 3 && r.archive.empty() && r.archive.first_height() == 0);
    r.step(21);
    CHECK(r.archive.size() == 1);
  }

  std::printf("== chain tree: the whole family restarts ==\n");
  {
    ChainTreeSpec spec;
    spec.root.chain_id = 1;
    spec.root.blueprint = ChainBlueprint{"pos", "account", "round-robin", {}};
    spec.root.validators = {tree_validator(0x01)};
    ChainTreeSpec::Node child;
    child.chain_id = 42;
    child.validators = {tree_validator(0x10), tree_validator(0x11), tree_validator(0x12)};
    ChainTreeSpec::Node grandchild;
    grandchild.chain_id = 420;
    grandchild.validators = {tree_validator(0x20), tree_validator(0x21)};
    grandchild.accounts = {GenesisAccount{bob, 1000}};
    child.children = {grandchild};
    ChainTreeSpec::Node sibling;
    sibling.chain_id = 43;
    sibling.validators = {tree_validator(0x30)};
    spec.root.children = {child, sibling};

    auto straight = ChainTree::build(spec);
    auto first = ChainTree::build(spec);
    auto round = [](ChainTree& t, std::uint64_t ts) {
      std::vector<Bytes> out;
      for (std::uint64_t id : {std::uint64_t{420}, std::uint64_t{43}, std::uint64_t{42}, std::uint64_t{1}})
        out.push_back(t.produce(id, ts).encode());
      return out;
    };
    CHECK(round(*straight, 10) == round(*first, 10));
    CHECK(round(*straight, 11) == round(*first, 11));
    const Bytes snap = first->snapshot();
    CHECK(snap == straight->snapshot());
    first.reset();
    auto second = ChainTree::restore(spec, v(snap));
    CHECK(second->snapshot() == snap);
    CHECK(second->height(1) == 2 && second->height(420) == 2);
    CHECK(round(*straight, 12) == round(*second, 12));
    CHECK(second->root_tip() == straight->root_tip());
    SubchainProof proof = second->prove_account(420, bob);
    CHECK(second->verify(proof) && straight->verify(proof));
    CHECK(second->snapshot() == straight->snapshot());

    Bytes trailing = snap;
    trailing.push_back(0);
    CHECK_THROWS(ChainTree::restore(spec, v(trailing)));
    CHECK_THROWS(ChainTree::restore(spec, v(Bytes(snap.begin(), snap.end() - 1))));
    Bytes magic = snap;
    magic[3] ^= 1;
    CHECK_THROWS(ChainTree::restore(spec, v(magic)));
    ChainTreeSpec smaller = spec;
    smaller.root.children.pop_back();
    CHECK_THROWS(ChainTree::restore(smaller, v(snap)));
    ChainTreeSpec different = spec;
    different.root.children[1].validators = {tree_validator(0x31)};
    CHECK_THROWS(ChainTree::restore(different, v(snap)));

    auto stale = ChainTree::build(spec);
    round(*stale, 10);
    const Bytes old_snap = stale->snapshot();
    codec::CodecReader fresh_reader(v(snap));
    codec::CodecReader old_reader(v(old_snap));
    codec::CodecWriter mixed;
    for (int i = 0; i < 5; ++i) {
      mixed.write_u8(fresh_reader.read_u8());
      (void)old_reader.read_u8();
    }
    const std::uint32_t count = fresh_reader.read_u32();
    (void)old_reader.read_u32();
    mixed.write_u32(count);
    for (std::uint32_t i = 0; i < count; ++i) {
      const std::uint64_t id = fresh_reader.read_u64();
      (void)old_reader.read_u64();
      const Bytes fresh_chain = fresh_reader.read_bytes();
      const Bytes old_chain = old_reader.read_bytes();
      mixed.write_u64(id);
      mixed.write_bytes(v(id == 420 ? old_chain : fresh_chain));
    }
    CHECK_THROWS(ChainTree::restore(spec, v(mixed.take())));
  }

  return bctest::summary();
}
