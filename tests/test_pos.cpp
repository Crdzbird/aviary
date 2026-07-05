// Proof-of-stake reference engine: a third consensus family behind the same
// ConsensusEngine seam. Each height has one deterministic, stake-weighted slot
// leader; the block is justified by that leader's signature. Proposer selection
// is an injectable policy (decoupling / enhancement seam).
#include <map>
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/pos_engine.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 7;

ValidatorSet stakes(const std::vector<std::pair<std::uint8_t, std::uint64_t>>& seed_stake) {
  std::vector<Validator> vs;
  for (auto& [s, stk] : seed_stake) vs.push_back(Validator{bls(s).public_key, stk});
  return ValidatorSet(std::move(vs));
}

std::unique_ptr<Ledger> genesis(const Hash& alice_addr) {
  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000000);
  return std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
}
}  // namespace

int main() {
  init();
  const Hash alice_addr = crypto::address_of(view(ed(1).public_key));

  std::printf("== stake-weighted selection: leadership ~ stake, 0-stake never leads ==\n");
  {
    // Stakes 1, 2, 7 (total 10); index 3 is slashed (0 stake).
    ValidatorSet vset = stakes({{10, 1}, {11, 2}, {12, 7}, {13, 0}});
    std::map<std::uint32_t, int> counts;
    for (std::uint64_t h = 1; h <= 10000; ++h) counts[stake_weighted_proposer(h, vset)]++;
    // Deterministic + within a sane band of the 10/20/70 split.
    CHECK(counts[3] == 0);                                  // slashed never leads
    CHECK(counts[2] > counts[1] && counts[1] > counts[0]);  // more stake → more slots
    CHECK(counts[2] > 6000 && counts[2] < 8000);            // ~70%
    CHECK(counts[0] > 600 && counts[0] < 1400);             // ~10%
    // Determinism: same height → same leader.
    CHECK(stake_weighted_proposer(42, vset) == stake_weighted_proposer(42, vset));
  }

  std::printf("== 3-validator chain: the height's leader proposes, all commit ==\n");
  {
    ValidatorSet vset = stakes({{10, 1}, {11, 1}, {12, 1}});
    std::vector<crypto::KeyPair> keys = {bls(10), bls(11), bls(12)};
    std::vector<std::unique_ptr<PosEngine>> nodes;
    for (std::uint32_t i = 0; i < 3; ++i) {
      nodes.push_back(std::make_unique<PosEngine>(vset, i, keys[i], genesis(alice_addr)));
    }
    for (std::uint64_t h = 1; h <= 6; ++h) {
      const std::uint32_t leader = stake_weighted_proposer(h, vset);
      // Only the leader may propose; a non-leader throws.
      const std::uint32_t other = (leader + 1) % 3;
      CHECK(::bctest::throws([&] { (void)nodes[other]->propose({}, h); }));

      Block b = nodes[leader]->propose({}, h);
      const Bytes sig = nodes[leader]->seal(b);
      // Every node accepts the leader's justified block.
      for (auto& n : nodes) CHECK(n->submit(b, view(sig)).ok);
    }
    // All ledgers agree on the same tip at height 6.
    const Hash tip = nodes[0]->tip().hash();
    for (auto& n : nodes) {
      CHECK(n->height() == 6);
      CHECK(n->tip().hash() == tip);
    }
  }

  std::printf("== adversarial: wrong leader / bad signature / impostor index rejected ==\n");
  {
    ValidatorSet vset = stakes({{10, 1}, {11, 1}, {12, 1}});
    const std::uint32_t leader = stake_weighted_proposer(1, vset);
    const std::uint32_t impostor = (leader + 1) % 3;

    PosEngine lead(vset, leader, bls(static_cast<std::uint8_t>(10 + leader)), genesis(alice_addr));
    PosEngine verifier(vset, 0, bls(10), genesis(alice_addr));

    Block b = lead.propose({}, 1);
    const Bytes good_sig = lead.seal(b);
    CHECK(verifier.submit(b, view(good_sig)).ok);  // legitimate

    // An impostor claims to be the proposer (wrong index in consensus_data).
    PosEngine verifier2(vset, 0, bls(10), genesis(alice_addr));
    Block forged = b;
    forged.header.consensus_data = PosData{impostor}.encode();
    // Re-sign as the impostor over the forged block id.
    PosEngine imp(vset, impostor, bls(static_cast<std::uint8_t>(10 + impostor)), genesis(alice_addr));
    // imp isn't the leader for height 1, so it can't propose — but it can still
    // try to sign a forged block; submit must reject (wrong leader for height).
    const Bytes imp_sig = imp.seal(forged);
    CHECK(!verifier2.submit(forged, view(imp_sig)).ok);

    // A tampered signature on the genuine block is rejected.
    PosEngine verifier3(vset, 0, bls(10), genesis(alice_addr));
    Bytes bad = good_sig;
    bad[bad.size() / 2] ^= 0x01;
    CHECK(!verifier3.submit(b, view(bad)).ok);
  }

  std::printf("== injectable policy: swap selection to round-robin ==\n");
  {
    ValidatorSet vset = stakes({{10, 5}, {11, 5}, {12, 5}});
    ProposerPolicy round_robin = [](std::uint64_t h, const ValidatorSet& v) {
      return static_cast<std::uint32_t>((h - 1) % v.size());
    };
    std::vector<crypto::KeyPair> keys = {bls(10), bls(11), bls(12)};
    std::vector<std::unique_ptr<PosEngine>> nodes;
    for (std::uint32_t i = 0; i < 3; ++i) {
      nodes.push_back(
          std::make_unique<PosEngine>(vset, i, keys[i], genesis(alice_addr), round_robin));
    }
    // Heights 1,2,3 → leaders 0,1,2 in strict rotation.
    for (std::uint64_t h = 1; h <= 3; ++h) {
      const std::uint32_t leader = static_cast<std::uint32_t>((h - 1) % 3);
      CHECK(nodes[leader]->proposer_for(h) == leader);
      Block b = nodes[leader]->propose({}, h);
      const Bytes sig = nodes[leader]->seal(b);
      for (auto& n : nodes) CHECK(n->submit(b, view(sig)).ok);
    }
    for (auto& n : nodes) CHECK(n->height() == 3);
  }

  std::printf("== drive through the abstract ConsensusEngine interface ==\n");
  {
    ValidatorSet vset = stakes({{10, 1}, {11, 1}});
    const std::uint32_t leader = stake_weighted_proposer(1, vset);
    PosEngine engine(vset, leader, bls(static_cast<std::uint8_t>(10 + leader)), genesis(alice_addr));
    ConsensusEngine& ce = engine;
    Block b = ce.propose({}, 1);
    const Bytes sig = engine.seal(b);  // sealing is PoS-specific
    CHECK_MSG(ce.submit(b, view(sig)).ok, "PoS block committed via the interface");
    CHECK(ce.height() == 1);
  }

  return bctest::summary();
}
