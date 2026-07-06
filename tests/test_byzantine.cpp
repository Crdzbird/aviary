// Adversarial gossip: does the honest network stay SAFE and LIVE when one
// validator is Byzantine? Two scenarios over the in-memory gossip harness:
//   1. Equivocation — the round-0 proposer double-proposes (two validly-signed,
//      conflicting blocks to different peers), then goes silent. The honest
//      quorum must still agree on ONE block (safety) and finalize it (liveness).
//   2. Spam — a peer floods undecodable garbage. Honest nodes must drop it, the
//      seen-set must bound the flood (dedup on re-inject), and consensus must
//      finish unaffected.
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/networked_node.hpp"
#include "blockchainer/round_driver.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;
using p2p::NetworkedNode;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 42;

ValidatorSet make_vset(std::uint32_t n) {
  std::vector<Validator> vs;
  for (std::uint32_t i = 0; i < n; ++i) vs.push_back(Validator{bls(static_cast<std::uint8_t>(0x60 + i)).public_key, 1});
  return ValidatorSet(std::move(vs));
}

std::unique_ptr<Ledger> genesis_ledger(const Hash& alice, std::uint64_t amount = 1000) {
  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice, amount);
  return std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
}

// A deterministic gossip harness. `nodes` has one slot per validator id; a null
// slot is a Byzantine/absent node the harness routes around. Relays addressed to
// a null slot are dropped (that node is silent).
struct Net {
  std::uint32_t n;
  std::vector<std::unique_ptr<NetworkedNode>> nodes;
  std::deque<std::tuple<std::uint32_t, std::uint32_t, Bytes>> queue;  // (from, to, payload)
  std::set<std::tuple<std::uint32_t, int, std::uint32_t>> timeouts;   // (round, step, node)

  explicit Net(std::uint32_t n_) : n(n_), nodes(n_) {}

  void apply(std::uint32_t node, const NetworkedNode::Outbound& o) {
    for (const auto& r : o.relays) queue.push_back({node, r.to, r.message});
    for (const auto& t : o.timeouts) timeouts.insert({t.round, static_cast<int>(t.step), node});
  }

  void inject(std::uint32_t from, std::uint32_t to, const Bytes& payload) {
    if (to < n && nodes[to]) apply(to, nodes[to]->on_gossip(from, payload));
  }

  void run(const std::function<bool()>& done, int budget = 300000) {
    for (int step = 0; step < budget; ++step) {
      if (done()) return;
      if (!queue.empty()) {
        auto [from, to, payload] = queue.front();
        queue.pop_front();
        if (to < n && nodes[to]) apply(to, nodes[to]->on_gossip(from, payload));
      } else if (!timeouts.empty()) {
        auto [round, tstep, node] = *timeouts.begin();
        timeouts.erase(timeouts.begin());
        if (nodes[node]) apply(node, nodes[node]->on_timeout(static_cast<Step>(tstep), round));
      } else {
        return;  // quiescent
      }
    }
  }
};
}  // namespace

int main() {
  init();
  const Hash alice = crypto::address_of(view(ed(1).public_key));
  const std::uint32_t n = 4;  // quorum = 3; tolerates f = 1 Byzantine
  const ValidatorSet vset = make_vset(n);

  std::printf("== equivocating proposer: honest quorum agrees on ONE block ==\n");
  {
    // Honest validators 1,2,3 (NetworkedNodes); validator 0 is Byzantine and is
    // the round-0 proposer. It builds TWO different, validly-signed proposals
    // (two RoundDrivers with different timestamps) and feeds A to nodes 1,2 and
    // B to node 3, then falls silent.
    Net net(n);
    for (std::uint32_t i = 1; i < n; ++i) {
      net.nodes[i] = std::make_unique<NetworkedNode>(kChain, vset, i, bls(static_cast<std::uint8_t>(0x60 + i)),
                                                     genesis_ledger(alice), /*timestamp=*/100);
      for (std::uint32_t j = 0; j < n; ++j)
        if (j != i) net.nodes[i]->add_peer(j);  // still peers with 0 — it just never answers
    }

    // Byzantine node 0: two proposals for the same (height 1, round 0). The
    // ledgers outlive the drivers (they hold raw pointers).
    auto ledA = genesis_ledger(alice);
    auto ledB = genesis_ledger(alice);
    RoundDriver evilA(kChain, vset, /*me=*/0, bls(0x60), ledA.get(), /*ts=*/7);
    RoundDriver evilB(kChain, vset, /*me=*/0, bls(0x60), ledB.get(), /*ts=*/9);
    RoundDriver::Output outA = evilA.start();
    RoundDriver::Output outB = evilB.start();
    CHECK_MSG(!outA.broadcasts.empty() && !outB.broadcasts.empty(), "the Byzantine proposer produced two proposals");
    const Bytes propA = outA.broadcasts[0].encode();
    const Bytes propB = outB.broadcasts[0].encode();
    CHECK_MSG(propA != propB, "the two proposals are genuinely different (conflicting)");

    net.inject(0, 1, propA);  // nodes 1,2 hear block A ...
    net.inject(0, 2, propA);
    net.inject(0, 3, propB);  // ... node 3 hears block B

    auto honest_committed = [&] {
      for (std::uint32_t i = 1; i < n; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(honest_committed);

    CHECK_MSG(honest_committed(), "the honest quorum finalized despite the equivocation (liveness)");
    const Hash tip = net.nodes[1]->ledger().tip().hash();
    for (std::uint32_t i = 2; i < n; ++i)
      CHECK_MSG(net.nodes[i]->ledger().tip().hash() == tip, "all honest nodes committed the SAME block (safety)");
    for (std::uint32_t i = 1; i < n; ++i) CHECK(net.nodes[i]->ledger().tip().height == 1);
  }

  std::printf("== gossip spam: garbage is dropped + deduped; consensus is unaffected ==\n");
  {
    // Four honest validators. The harness floods undecodable garbage into the
    // mesh interleaved with real consensus.
    Net net(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      net.nodes[i] = std::make_unique<NetworkedNode>(kChain, vset, i, bls(static_cast<std::uint8_t>(0x60 + i)),
                                                     genesis_ledger(alice), /*timestamp=*/100);
      for (std::uint32_t j = 0; j < n; ++j)
        if (j != i) net.nodes[i]->add_peer(j);
    }
    for (std::uint32_t i = 0; i < n; ++i) net.apply(i, net.nodes[i]->start());

    // A garbage payload that cannot decode as a wire::Message.
    auto garbage = [](std::uint8_t tag) { return Bytes(40, tag); };

    // Inject garbage from every peer into every node before draining.
    for (std::uint32_t to = 0; to < n; ++to)
      for (std::uint32_t from = 0; from < n; ++from)
        if (from != to) net.inject(from, to, garbage(static_cast<std::uint8_t>(0xF0 + from)));

    // Re-injecting the SAME garbage is deduped by the seen-set: a second delivery
    // produces no new relays (the flood is bounded to one pass).
    const Bytes g = garbage(0xF0);
    net.nodes[1]->on_gossip(0, g);  // first delivery already happened above; ensure it's seen
    NetworkedNode::Outbound again = net.nodes[1]->on_gossip(0, g);
    CHECK_MSG(again.relays.empty(), "re-delivered garbage is deduped (seen-set bounds the flood)");

    auto all_committed = [&] {
      for (std::uint32_t i = 0; i < n; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(all_committed);
    CHECK_MSG(all_committed(), "every honest node committed despite the spam");
    const Hash tip = net.nodes[0]->ledger().tip().hash();
    for (std::uint32_t i = 1; i < n; ++i) CHECK(net.nodes[i]->ledger().tip().hash() == tip);
  }

  return bctest::summary();
}
