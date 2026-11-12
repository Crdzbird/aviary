#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/dynamic_subchain.hpp"
#include "aviary/genesis.hpp"
#include "aviary/ledger.hpp"
#include "aviary/merkle.hpp"
#include "aviary/subchain.hpp"
#include "aviary/subchain_relay.hpp"

/// Shared fixtures for the dynamic subchain tests: a registrar key, a child
/// chain that finalizes real certified headers, and a parent chain with a
/// dynamic policy whose every block is also replayed by an independent
/// follower ledger.
namespace dyntest {

using namespace aviary;

inline codec::ByteView v(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
inline codec::ByteView hv(const Hash& h) { return codec::ByteView(h.data(), h.size()); }
inline Hash fill(std::uint8_t x) {
  Hash h;
  h.fill(x);
  return h;
}
inline crypto::KeyPair bls(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return crypto::bls_keygen_from_ikm(v(ikm));
}
inline crypto::KeyPair ed(std::uint8_t seed) {
  Bytes s(32, seed);
  return crypto::ed25519_keygen_from_seed(v(s));
}
inline Registrar ed_registrar(const crypto::KeyPair& k) {
  return Registrar{static_cast<std::uint8_t>(SigAlgo::kEd25519), k.public_key};
}

/// A BFT child chain with one validator.
struct Child {
  GenesisSpec genesis;
  crypto::KeyPair key;
  std::unique_ptr<consensus::BftEngine> engine;
  BlockStore archive;

  Child(std::uint64_t chain_id, std::uint8_t seed, const Hash* funded = nullptr) : key(bls(seed)) {
    genesis.chain_id = chain_id;
    genesis.timestamp = 1;
    genesis.validators = {consensus::Validator{key.public_key, 1}};
    if (funded != nullptr) genesis.accounts = {GenesisAccount{*funded, 500}};
    boot();
  }
  Child(GenesisSpec g, crypto::KeyPair k) : genesis(std::move(g)), key(std::move(k)) { boot(); }
  void boot() {
    engine = std::make_unique<consensus::BftEngine>(genesis.validator_set(), ledger_from_genesis(genesis));
  }
  ChildRegistration registration(std::uint8_t depth = 0) const {
    ChildRegistration c;
    c.chain_id = genesis.chain_id;
    c.genesis_header = ledger_from_genesis(genesis)->tip();
    c.validators = genesis.validators;
    c.child_max_depth = depth;
    return c;
  }
  CertifiedHeader next(std::uint64_t ts, std::vector<Anchor> anchors = {}, std::vector<SubchainOp> ops = {}) {
    Block b = engine->propose({}, ts, std::move(anchors), std::move(ops));
    auto cert = engine->try_commit(b, {engine->precommit(b, 0, key)});
    if (!cert) throw std::runtime_error("child: no quorum");
    if (BlockError e = engine->finalize(b, *cert); !e.ok) throw std::runtime_error("child finalize: " + e.error);
    archive.put(b, *cert);
    return CertifiedHeader{b.header, cert->encode()};
  }
};

/// A BFT parent chain with a dynamic policy and one validator, plus a
/// follower ledger that replays every block it produces.
struct Parent {
  GenesisSpec genesis;
  Hash genesis_hash{};
  crypto::KeyPair key;
  std::unique_ptr<consensus::BftEngine> engine;
  std::unique_ptr<Ledger> follower;
  BlockStore archive;

  Parent(std::uint64_t chain_id, const DynamicSubchainPolicy& policy, std::uint64_t timestamp = 1,
         std::uint8_t seed = 0x01)
      : key(bls(seed)) {
    genesis.chain_id = chain_id;
    genesis.timestamp = timestamp;
    genesis.validators = {consensus::Validator{key.public_key, 1}};
    policy.commit_to(genesis);
    genesis_hash = genesis.hash();
    engine = std::make_unique<consensus::BftEngine>(genesis.validator_set(), ledger_from_genesis(genesis));
    follower = ledger_from_genesis(genesis);
  }
  const DynamicSubchainRegistry& reg() const { return *engine->ledger().dynamic_subchains(); }
  const BlockHeader& tip() const { return engine->tip(); }

  Block produce(std::uint64_t ts, std::vector<Anchor> anchors = {}, std::vector<SubchainOp> ops = {}) {
    Block b = engine->propose({}, ts, std::move(anchors), std::move(ops));
    auto cert = engine->try_commit(b, {engine->precommit(b, 0, key)});
    if (!cert) throw std::runtime_error("parent: no quorum");
    if (BlockError e = engine->finalize(b, *cert); !e.ok) throw std::runtime_error("parent finalize: " + e.error);
    if (BlockError e = follower->process_block(Block::decode_all(v(b.encode()))); !e.ok)
      throw std::runtime_error("follower: " + e.error);
    archive.put(b, *cert);
    return b;
  }
  /// The reason the ledger refuses to build a block with this content, or "".
  std::string refuses(std::uint64_t ts, std::vector<Anchor> anchors, std::vector<SubchainOp> ops) {
    try {
      (void)engine->propose({}, ts, std::move(anchors), std::move(ops));
      return {};
    } catch (const std::exception& e) {
      return e.what();
    }
  }
  SubchainOp sign(SubchainOp op, const std::vector<std::pair<std::uint16_t, const crypto::KeyPair*>>& signers) const {
    for (const auto& [index, k] : signers)
      approve_op(op, genesis.chain_id, genesis_hash, index, SigAlgo::kEd25519, v(k->secret_key));
    return op;
  }
  std::vector<Anchor> anchors_for(const std::vector<const Child*>& children) const {
    std::vector<Anchor> out;
    for (const Child* c : children)
      if (auto a = node::pending_anchor(reg(), c->genesis.chain_id, c->archive)) out.push_back(std::move(*a));
    std::sort(out.begin(), out.end(),
              [](const Anchor& x, const Anchor& y) { return x.child_chain_id < y.child_chain_id; });
    return out;
  }
};

/// The registry root recomputed from scratch from the records alone.
inline Hash naive_root(const DynamicSubchainRegistry& r) {
  std::vector<Hash> leaves;
  std::uint32_t open = 0;
  for (std::uint32_t i = 0; i < r.record_count(); ++i) {
    const ChildRecord& rec = r.child_at(i)->record;
    leaves.push_back(rec.leaf());
    if (rec.status == ChildRecord::kOpen) ++open;
  }
  return dynamic_registry_root(r.record_count(), open, r.authority_epoch(), r.authority().hash(),
                               merkle::compute_root(leaves));
}

inline bool contains(const std::string& haystack, const char* needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace dyntest
