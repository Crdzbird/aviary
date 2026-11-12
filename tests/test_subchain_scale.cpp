#include <cstdio>
#include <memory>
#include <vector>

#include "aviary/version.hpp"
#include "check.hpp"
#include "dynamic_support.hpp"

using namespace aviary;
using namespace dyntest;

namespace {
constexpr std::uint64_t kChildren = 2000;
constexpr std::uint64_t kPerBlock = 250;

struct LightChild {
  ChildRegistration registration;
  CertifiedHeader first;
};

LightChild light_child(std::uint64_t chain_id, const crypto::KeyPair& key) {
  GenesisSpec g;
  g.chain_id = chain_id;
  g.timestamp = 1;
  g.validators = {consensus::Validator{key.public_key, 1}};
  consensus::BftEngine engine(g.validator_set(), ledger_from_genesis(g));
  LightChild out;
  out.registration.chain_id = chain_id;
  out.registration.genesis_header = engine.tip();
  out.registration.validators = g.validators;
  const Block b = engine.propose({}, 5);
  const consensus::Vote vote = engine.precommit(b, 0, key);
  const auto cert = consensus::aggregate_commit(chain_id, engine.validators(), 1, 0, b.header.hash(), {vote}, true);
  out.first = CertifiedHeader{b.header, cert->encode()};
  return out;
}

std::size_t levels_for(std::uint64_t n) {
  std::size_t levels = 0;
  while ((std::uint64_t{1} << levels) < n) ++levels;
  return levels;
}
}  // namespace

int main() {
  init();
  const crypto::KeyPair registrar = ed(0xA0);
  const crypto::KeyPair child_key = bls(0x33);

  std::printf("== %llu children registered after genesis, anchored, and proven ==\n",
              static_cast<unsigned long long>(kChildren));
  DynamicSubchainPolicy policy;
  policy.max_depth = 1;
  policy.max_ops_per_block = static_cast<std::uint16_t>(kPerBlock);
  policy.max_anchors_per_block = static_cast<std::uint16_t>(kPerBlock);
  policy.authority.threshold = 1;
  policy.authority.registrars = {ed_registrar(registrar)};
  Parent parent(1, policy);
  CHECK(parent.reg().record_count() == 0);

  std::vector<LightChild> children;
  children.reserve(kChildren);
  for (std::uint64_t i = 0; i < kChildren; ++i) children.push_back(light_child(1000 + i, child_key));

  std::uint64_t ts = 10;
  std::size_t small_proof = 0;
  for (std::uint64_t from = 0; from < kChildren; from += kPerBlock) {
    std::vector<SubchainOp> ops;
    for (std::uint64_t i = from; i < from + kPerBlock; ++i)
      ops.push_back(parent.sign(make_register_op(0, 0, children[i].registration), {{0, &registrar}}));
    parent.produce(ts++, {}, ops);
    if (from == 0) small_proof = parent.reg().prove(1000).encode().size();
  }
  CHECK(parent.reg().record_count() == kChildren && parent.reg().open_count() == kChildren);
  CHECK(parent.tip().height == kChildren / kPerBlock);

  for (std::uint64_t from = 0; from < kChildren; from += kPerBlock) {
    std::vector<Anchor> anchors;
    for (std::uint64_t i = from; i < from + kPerBlock; ++i) {
      Anchor a;
      a.child_chain_id = 1000 + i;
      a.headers = {children[i].first};
      anchors.push_back(std::move(a));
    }
    parent.produce(ts++, anchors, {});
  }
  CHECK(parent.reg().root() == naive_root(parent.reg()));
  CHECK(parent.follower->dynamic_subchains()->root() == parent.reg().root());
  CHECK(parent.tip().subchain_root == parent.reg().root());

  bool all_anchored = true, all_proven = true;
  std::size_t largest = 0;
  for (std::uint64_t i = 0; i < kChildren; i += 7) {
    const ChildProof proof = parent.reg().prove(1000 + i);
    const std::optional<ChildRecord> rec = verify_child_proof(parent.tip().subchain_root, proof);
    all_proven &= rec.has_value() && rec->chain_id == 1000 + i && rec->index == i;
    all_anchored &= rec.has_value() && rec->tip_height == 1 && rec->tip_hash == children[i].first.header.hash();
    largest = std::max(largest, proof.encode().size());
    all_proven &= proof.siblings.size() <= levels_for(kChildren);
  }
  CHECK_MSG(all_anchored, "every sampled child's anchored tip is the header it finalized");
  CHECK_MSG(all_proven, "every sampled child proof verifies against the parent header");

  const std::size_t fixed = ChildRecord::kEncodedSize + 4 + 4 + 4 + 4 + 32 + 4;
  std::printf("  proof size: %zu bytes at %llu children, %zu bytes at %llu children\n", small_proof,
              static_cast<unsigned long long>(kPerBlock), largest, static_cast<unsigned long long>(kChildren));
  CHECK(small_proof <= fixed + 32 * levels_for(kPerBlock));
  CHECK(largest <= fixed + 32 * levels_for(kChildren));
  CHECK_MSG(largest - small_proof <= 32 * (levels_for(kChildren) - levels_for(kPerBlock) + 1),
            "eight times the children costs a few more hashes, not eight times the bytes");

  std::vector<ChildSummary> flat;
  for (std::uint64_t i = 0; i < kChildren; ++i) {
    const DynamicSubchainRegistry::Child& c = *parent.reg().child_at(static_cast<std::uint32_t>(i));
    flat.push_back(ChildSummary{c.record.chain_id, c.record.genesis_header_hash, c.tip, c.record.vset_hash,
                                c.record.tip_vset_hash});
  }
  const std::size_t flat_hop = SubchainProof::Hop{flat, 1000}.encode().size();
  std::printf("  the version 1 flat hop for the same registry would be %zu bytes\n", flat_hop);
  CHECK(flat_hop > 100 * largest);

  std::printf("== the registry survives a snapshot at this size and one more block ==\n");
  {
    const Bytes snap = parent.engine->ledger().snapshot();
    auto restored = Ledger::restore(parent.genesis.chain_spec(), v(snap));
    CHECK(restored->dynamic_subchains()->root() == parent.reg().root());
    CHECK(restored->snapshot() == snap);
    const LightChild late = light_child(9000, child_key);
    Block next = parent.produce(ts++, {}, {parent.sign(make_register_op(0, 0, late.registration), {{0, &registrar}})});
    next.header.consensus_data.clear();
    Block rebuilt = restored->build_block({}, next.header.timestamp, {}, next.ops);
    CHECK(rebuilt.header.subchain_root == next.header.subchain_root && rebuilt.header.ops_root == next.header.ops_root);
    CHECK(parent.reg().child(9000)->record.index == kChildren);
  }

  return bctest::summary();
}
