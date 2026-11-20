#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/assurance.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/block_store.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/chain_snapshot.hpp"
#include "aviary/genesis.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using consensus::CommitCertificate;
using consensus::ValidatorSet;
using node::ChainSnapshot;

namespace {
constexpr std::uint64_t kChain = 11;

codec::ByteView v(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

crypto::KeyPair bls(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return crypto::bls_keygen_from_ikm(v(ikm));
}

std::vector<consensus::Validator> members(const std::vector<crypto::KeyPair>& keys) {
  std::vector<consensus::Validator> out;
  for (const crypto::KeyPair& k : keys) out.push_back(consensus::Validator{k.public_key, 1});
  return out;
}

GenesisSpec genesis_for(const std::vector<crypto::KeyPair>& keys) {
  GenesisSpec g;
  g.chain_id = kChain;
  g.timestamp = 1;
  Hash funded;
  funded.fill(0xA1);
  g.accounts = {GenesisAccount{funded, 1000}};
  g.validators = members(keys);
  AssuranceProfile::standard().apply_to(g);
  ChainBlueprint{"bft", "account", "", {}}.commit_to(g);
  return g;
}

struct Chain {
  GenesisSpec genesis;
  std::vector<crypto::KeyPair> keys;
  node::AssembledChain chain;
  BlockStore archive;
  std::vector<Block> blocks;

  Chain(GenesisSpec g, std::vector<crypto::KeyPair> k)
      : genesis(std::move(g)), keys(std::move(k)), chain(node::ChainAssembler::assemble(genesis, 0, keys[0])) {}
  consensus::BftEngine& bft() { return dynamic_cast<consensus::BftEngine&>(*chain.engine); }

  Bytes step(std::uint64_t ts) {
    Block b = bft().propose({}, ts);
    std::vector<consensus::Vote> votes;
    for (const crypto::KeyPair& k : keys)
      if (auto idx = bft().validators().index_of(k.public_key)) votes.push_back(bft().precommit(b, *idx, k));
    auto cert = bft().try_commit(b, votes);
    if (!cert) throw std::runtime_error("no quorum");
    const Bytes just = cert->encode();
    if (BlockError e = chain.engine->submit(b, v(just)); !e.ok) throw std::runtime_error(e.error);
    archive.put(b, *cert);
    blocks.push_back(b);
    return b.encode();
  }
  Bytes snapshot() const { return node::snapshot_chain(genesis.hash(), chain, &archive); }
};

std::string restore_error(const GenesisSpec& g, const Bytes& snapshot) {
  try {
    (void)node::restore_chain(g, 0, crypto::KeyPair{}, v(snapshot));
    return {};
  } catch (const std::exception& e) {
    return std::string("rejected: ") + e.what();
  }
}

/// [snapshot] with its archive cut to the heights from [first] on and the
/// certificate at [height] replaced by the raw bytes [cert] (none when 0).
Bytes rewrite(const Bytes& snapshot, std::uint64_t first, std::uint64_t height, const Bytes& cert) {
  ChainSnapshot s = ChainSnapshot::decode_all(v(snapshot));
  const BlockStore stored = BlockStore::load(v(*s.archive));
  const std::uint64_t from = std::max(first, stored.first_height());
  codec::CodecWriter w;
  w.write_u32(static_cast<std::uint32_t>(stored.tip_height() - from + 1));
  for (std::uint64_t h = from; h <= stored.tip_height(); ++h) {
    const StoredBlock& sb = stored.by_height(h);
    const Bytes blk = sb.block.encode();
    const Bytes own = sb.commit.encode();
    w.write_u64(h);
    w.write_bytes(v(blk));
    w.write_bytes(v(h == height ? cert : own));
  }
  s.archive = w.take();
  return s.encode();
}

/// A quorum certificate for [b] under [set] from [signers] only.
CommitCertificate quorum(const ValidatorSet& set, const Block& b, const std::vector<crypto::KeyPair>& signers) {
  std::vector<consensus::Vote> votes;
  for (const crypto::KeyPair& k : signers)
    votes.push_back(consensus::sign_vote(kChain, b.header.height, 0, b.header.hash(), *set.index_of(k.public_key), k));
  auto cert = consensus::aggregate_commit(kChain, set, b.header.height, 0, b.header.hash(), votes);
  if (!cert) throw std::runtime_error("no quorum");
  return *cert;
}

/// How many single byte flips of the certificate at [height] restore accepts.
int accepted_flips(const GenesisSpec& g, const Bytes& snapshot, std::uint64_t first, std::uint64_t height) {
  const BlockStore stored = BlockStore::load(v(*ChainSnapshot::decode_all(v(snapshot)).archive));
  const Bytes cert = stored.by_height(height).commit.encode();
  int accepted = 0;
  for (std::size_t i = 0; i < cert.size(); ++i) {
    Bytes flipped = cert;
    flipped[i] ^= 0x01;
    if (restore_error(g, rewrite(snapshot, first, height, flipped)).empty()) ++accepted;
  }
  return accepted;
}
}  // namespace

int main() {
  init();
  std::vector<crypto::KeyPair> k;
  for (std::uint8_t i = 0; i < 8; ++i) k.push_back(bls(static_cast<std::uint8_t>(0x40 + i)));
  const std::vector<crypto::KeyPair> s0_keys = {k[0], k[1], k[2], k[3]};
  const std::vector<crypto::KeyPair> s1_keys = {k[1], k[2], k[3], k[4]};
  const std::vector<crypto::KeyPair> s2_keys = {k[4], k[5], k[6], k[7]};
  const ValidatorSet s0(members(s0_keys)), s1(members(s1_keys)), s2(members(s2_keys));
  const GenesisSpec genesis = genesis_for(s0_keys);

  Chain chain(genesis, k);
  chain.step(10);
  chain.bft().schedule_validator_set(s1);
  chain.step(11);
  chain.bft().schedule_validator_set(s2);
  chain.step(12);
  chain.step(13);
  chain.step(14);
  const Bytes snap = chain.snapshot();
  const std::vector<Block>& blocks = chain.blocks;

  std::printf("== the chain rotated twice: heights 1-2 under S0, 3 under S1, 4-5 under S2 ==\n");
  {
    auto set_hash = [&](std::uint64_t h) {
      const BlockHeader& header = chain.archive.by_height(h).block.header;
      return consensus::ConsensusData::decode_all(v(header.consensus_data)).validator_set_hash;
    };
    CHECK(set_hash(1) == s0.hash() && set_hash(2) == s0.hash());
    CHECK(set_hash(3) == s1.hash());
    CHECK(set_hash(4) == s2.hash() && set_hash(5) == s2.hash());
    CHECK(chain.bft().validators().hash() == s2.hash() && chain.bft().tip_validators().hash() == s2.hash());
    CHECK(rewrite(snap, 0, 0, {}) == snap);
  }

  std::printf("== an honest snapshot restores byte for byte across both rotations ==\n");
  {
    CHECK(restore_error(genesis, snap).empty());
    node::RestoredChain back = node::restore_chain(genesis, 0, k[4], v(snap));
    CHECK(back.archive && back.archive->serialize() == chain.archive.serialize());
    CHECK(node::snapshot_chain(genesis.hash(), back.chain, &*back.archive) == snap);
    for (std::uint64_t h = 1; h <= 5; ++h)
      CHECK(back.archive->by_height(h).commit.encode() == chain.archive.by_height(h).commit.encode());
  }

  std::printf("== every flipped byte of every archived certificate is refused ==\n");
  for (std::uint64_t h = 1; h <= 5; ++h) {
    const int accepted = accepted_flips(genesis, snap, 0, h);
    std::printf("  height %llu: %d accepted flips\n", static_cast<unsigned long long>(h), accepted);
    CHECK(accepted == 0);
  }

  std::printf("== a forged certificate below the tip names its height ==\n");
  {
    CommitCertificate forged = chain.archive.by_height(1).commit;
    forged.agg_signature = chain.archive.by_height(2).commit.agg_signature;
    const std::string e1 = restore_error(genesis, rewrite(snap, 0, 1, forged.encode()));
    CHECK(e1.find("height 1") != std::string::npos && e1.find("aggregate signature") != std::string::npos);

    CommitCertificate thin = chain.archive.by_height(3).commit;
    thin.signers.resize(2);
    const std::string e3 = restore_error(genesis, rewrite(snap, 0, 3, thin.encode()));
    CHECK(e3.find("height 3") != std::string::npos && e3.find("quorum") != std::string::npos);

    const CommitCertificate wrong_set = quorum(s0, blocks[2], {k[0], k[1], k[2]});
    CHECK(!restore_error(genesis, rewrite(snap, 0, 3, wrong_set.encode())).empty());
  }

  std::printf("== another valid quorum is accepted under the set of its height ==\n");
  for (const auto& [h, set, signers] :
       std::vector<std::tuple<std::uint64_t, ValidatorSet, std::vector<crypto::KeyPair>>>{
           {2, s0, {k[1], k[2], k[3]}}, {3, s1, {k[2], k[3], k[4]}}, {4, s2, {k[4], k[5], k[7]}}}) {
    const CommitCertificate other = quorum(set, blocks[h - 1], signers);
    CHECK(other.encode() != chain.archive.by_height(h).commit.encode());
    const Bytes swapped = rewrite(snap, 0, h, other.encode());
    CHECK(restore_error(genesis, swapped).empty());
    node::RestoredChain back = node::restore_chain(genesis, 0, k[4], v(swapped));
    CHECK(node::snapshot_chain(genesis.hash(), back.chain, &*back.archive) == swapped);
  }

  std::printf("== a pruned archive whose oldest set is no longer in the snapshot ==\n");
  {
    const Bytes pruned = rewrite(snap, 3, 0, {});
    CHECK(BlockStore::load(v(*ChainSnapshot::decode_all(v(pruned)).archive)).first_height() == 3);
    CHECK(restore_error(genesis, pruned).empty());
    node::RestoredChain back = node::restore_chain(genesis, 0, k[4], v(pruned));
    CHECK(node::snapshot_chain(genesis.hash(), back.chain, &*back.archive) == pruned);
    for (std::uint64_t h = 3; h <= 5; ++h) CHECK(accepted_flips(genesis, pruned, 3, h) == 0);

    const CommitCertificate other = quorum(s1, blocks[2], {k[2], k[3], k[4]});
    const std::string e = restore_error(genesis, rewrite(snap, 3, 3, other.encode()));
    CHECK(e.find("height 3") != std::string::npos && e.find("cannot be verified") != std::string::npos);
    CHECK(restore_error(genesis, rewrite(snap, 2, 3, other.encode())).empty());
  }

  std::printf("== a restored chain keeps producing what the original does ==\n");
  {
    Chain resumed(genesis, k);
    node::RestoredChain back = node::restore_chain(genesis, 0, k[4], v(snap));
    resumed.chain = std::move(back.chain);
    resumed.archive = std::move(*back.archive);
    CHECK(resumed.step(15) == chain.step(15));
    CHECK(resumed.snapshot() == chain.snapshot());
    CHECK(restore_error(genesis, chain.snapshot()).empty());
  }

  return bctest::summary();
}
