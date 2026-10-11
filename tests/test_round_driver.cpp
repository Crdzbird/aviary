// Integration: the async round state machine driven by RoundDriver over real
// crypto + ledger. N validators, each with its own Ledger, gossip signed
// wire::ConsensusMsg to each other; timeouts fire when the network is quiescent.
// We assert they all COMMIT the same real block (matching state_root), building a
// real >2/3 commit certificate - including a faulty-proposer round change.
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/round_driver.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) {
  Bytes ikm(32, s);
  return crypto::bls_keygen_from_ikm(view(ikm));
}
crypto::KeyPair ed(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::ed25519_keygen_from_seed(view(seed));
}

constexpr std::uint64_t kChain = 42;

// A deterministic in-memory network of RoundDrivers, each over its own ledger.
struct Net {
  std::uint32_t n;
  ValidatorSet vset;
  std::vector<std::unique_ptr<Ledger>> ledgers;
  std::vector<std::unique_ptr<RoundDriver>> drv;
  std::set<std::uint32_t> correct;
  std::deque<std::pair<std::uint32_t, wire::ConsensusMsg>> queue;  // (from, msg)
  std::set<std::tuple<std::uint32_t, int, std::uint32_t>> timeouts;  // (round, step, owner)

  static ValidatorSet make_vset(std::uint32_t n) {
    std::vector<Validator> vs;
    for (std::uint32_t i = 0; i < n; ++i) vs.push_back(Validator{bls(static_cast<std::uint8_t>(0x50 + i)).public_key, 1});
    return ValidatorSet(std::move(vs));
  }

  explicit Net(std::uint32_t n_, const Hash& alice_addr) : n(n_), vset(make_vset(n_)) {
    for (std::uint32_t i = 0; i < n; ++i) {
      auto g = std::make_unique<AccountStateMachine>();
      g->credit_genesis(alice_addr, 1000);
      ledgers.push_back(std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g)));
    }
    for (std::uint32_t i = 0; i < n; ++i) {
      drv.push_back(std::make_unique<RoundDriver>(kChain, vset, i,
                                                  bls(static_cast<std::uint8_t>(0x50 + i)),
                                                  ledgers[i].get(), /*timestamp=*/1));
      correct.insert(i);
    }
  }

  void apply(std::uint32_t from, const RoundDriver::Output& out) {
    for (const auto& m : out.broadcasts) queue.push_back({from, wire::as_consensus(m)});
    for (const auto& t : out.timeouts) timeouts.insert({t.round, static_cast<int>(t.step), from});
  }

  // Drain the network until every correct validator commits its current height.
  void pump(int budget = 200000) {
    int steps = 0;
    while (steps++ < budget) {
      bool all = true;
      for (std::uint32_t i : correct) all &= drv[i]->committed();
      if (all) return;
      if (!queue.empty()) {
        auto [from, msg] = queue.front();
        queue.pop_front();
        for (std::uint32_t i : correct) apply(i, drv[i]->deliver(msg));  // gossip to all
      } else if (!timeouts.empty()) {
        auto [round, step, owner] = *timeouts.begin();
        timeouts.erase(timeouts.begin());
        if (correct.count(owner)) apply(owner, drv[owner]->on_timeout(static_cast<Step>(step), round));
      } else {
        return;
      }
    }
  }

  void run() {
    for (std::uint32_t i : correct) apply(i, drv[i]->start());
    pump();
  }

  // Roll every validator into the next height and drive it to commit.
  void advance_all() {
    queue.clear();
    timeouts.clear();
    for (std::uint32_t i : correct) apply(i, drv[i]->advance());
    pump();
  }
};
}  // namespace

int main() {
  init();
  const Hash alice_addr = crypto::address_of(view(ed(1).public_key));

  std::printf("== 4 validators commit the same block via the round driver ==\n");
  {
    Net net(4, alice_addr);
    net.run();
    bool all_committed = true;
    for (std::uint32_t i = 0; i < 4; ++i) all_committed &= net.drv[i]->committed();
    CHECK(all_committed);

    // Every ledger advanced to height 1 with an identical state root.
    const Hash root0 = net.ledgers[0]->tip().state_root;
    bool agree = true;
    for (std::uint32_t i = 0; i < 4; ++i) {
      agree &= net.ledgers[i]->tip().height == 1;
      agree &= net.ledgers[i]->tip().state_root == root0;
    }
    CHECK_MSG(agree, "all ledgers committed the same block 1 (matching state root)");

    // The commit certificate is a real >2/3 aggregate that verifies.
    CHECK(net.drv[0]->certificate().has_value());
    CHECK(verify_commit(kChain, net.vset, *net.drv[0]->certificate()).ok);
  }

  std::printf("== faulty round-0 proposer: a later round commits, all agree ==\n");
  {
    Net net(4, alice_addr);
    net.correct.erase(0);  // validator 0 (round-0 proposer) is silent
    net.run();
    bool committed = true, agree = true;
    Hash root;
    bool have = false;
    for (std::uint32_t i : net.correct) {
      committed &= net.drv[i]->committed();
      agree &= net.ledgers[i]->tip().height == 1;
      if (!have) { root = net.ledgers[i]->tip().state_root; have = true; }
      else { agree &= net.ledgers[i]->tip().state_root == root; }
    }
    CHECK(committed);
    CHECK_MSG(agree, "the 3 honest validators committed the same block after a round change");
    // They advanced past the failed round 0.
    bool advanced = true;
    for (std::uint32_t i : net.correct) advanced &= net.drv[i]->round() >= 1;
    CHECK(advanced);
  }

  std::printf("== multi-height chain: LastCommit certifies each parent ==\n");
  {
    Net net(4, alice_addr);
    net.run();  // height 1
    CHECK(net.ledgers[0]->tip().height == 1);
    // Height 1 links to genesis and carries NO last commit.
    const Hash genesis_hash = net.ledgers[0]->tip().parent_hash;
    CHECK(validate_consensus_data(kChain, net.vset, net.ledgers[0]->tip(), genesis_hash).ok);

    for (std::uint64_t h = 2; h <= 4; ++h) {
      net.advance_all();
      // Every ledger advanced to h with the same tip hash + state root.
      const Hash tip_hash = net.ledgers[0]->tip().hash();
      const Hash root = net.ledgers[0]->tip().state_root;
      bool agree = true;
      for (std::uint32_t i = 0; i < 4; ++i) {
        agree &= net.drv[i]->committed();
        agree &= net.ledgers[i]->tip().height == h;
        agree &= net.ledgers[i]->tip().hash() == tip_hash;
        agree &= net.ledgers[i]->tip().state_root == root;
      }
      CHECK_MSG(agree, "all ledgers agree on the block at this height");

      // The block's embedded LastCommit is a valid >2/3 certificate for its parent.
      const BlockHeader& tip = net.ledgers[0]->tip();
      CHECK(validate_consensus_data(kChain, net.vset, tip, tip.parent_hash).ok);
      ConsensusData cd = ConsensusData::decode_all(
          codec::ByteView(tip.consensus_data.data(), tip.consensus_data.size()));
      CHECK(cd.last_commit.has_value());
      CHECK(cd.last_commit->height == h - 1);
      CHECK(cd.last_commit->block_id == tip.parent_hash);
      CHECK(verify_commit(kChain, net.vset, *cd.last_commit).ok);
    }
  }

  std::printf("== multi-height with a rotating faulty proposer still chains ==\n");
  {
    Net net(4, alice_addr);
    // Validator that is silent this run is index 3 (never the round-0 proposer of
    // heights 1..3, but exercises consensus with only 3 of 4 live).
    net.correct.erase(3);
    net.run();
    for (std::uint64_t h = 2; h <= 3; ++h) net.advance_all();
    bool ok = true;
    for (std::uint32_t i : net.correct) {
      ok &= net.drv[i]->committed();
      ok &= net.ledgers[i]->tip().height == 3;
    }
    CHECK_MSG(ok, "3-of-4 validators chained three heights");
  }

  std::printf("== 7 validators, silent minority (f=2): the 5 honest commit ==\n");
  {
    Net net(7, alice_addr);
    net.correct.erase(5);
    net.correct.erase(6);
    net.run();
    bool committed = true, agree = true;
    Hash root;
    bool have = false;
    for (std::uint32_t i : net.correct) {
      committed &= net.drv[i]->committed();
      if (!have) { root = net.ledgers[i]->tip().state_root; have = true; }
      else { agree &= net.ledgers[i]->tip().state_root == root; }
    }
    CHECK(committed);
    CHECK(agree);
  }

  return bctest::summary();
}
