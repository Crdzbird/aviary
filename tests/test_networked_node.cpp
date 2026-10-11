// A validator node = RoundDriver over GossipOverlay. Consensus runs over a
// realistic partial-mesh gossip network (relayed, deduped) instead of magic
// broadcast - so we can show relay tolerance (a validator reaches quorum via
// hops it isn't directly connected to) and partition tolerance (a connected
// >2/3 majority finalizes; an isolated minority stalls).
#include <deque>
#include <functional>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/ledger.hpp"
#include "aviary/networked_node.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;
using p2p::NetworkedNode;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 42;

// A deterministic network of NetworkedNodes over a gossip topology given as
// undirected edges. Consensus flows only along those edges (relayed + deduped).
struct Net {
  std::uint32_t n;
  ValidatorSet vset;
  std::vector<std::unique_ptr<NetworkedNode>> nodes;
  std::deque<std::tuple<std::uint32_t, std::uint32_t, Bytes>> queue;  // (from, to, payload)
  std::set<std::tuple<std::uint32_t, int, std::uint32_t>> timeouts;   // (round, step, node)

  static ValidatorSet make_vset(std::uint32_t n) {
    std::vector<Validator> vs;
    for (std::uint32_t i = 0; i < n; ++i) vs.push_back(Validator{bls(static_cast<std::uint8_t>(0x60 + i)).public_key, 1});
    return ValidatorSet(std::move(vs));
  }

  Net(std::uint32_t n_, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& edges,
      const Hash& alice_addr)
      : n(n_), vset(make_vset(n_)) {
    for (std::uint32_t i = 0; i < n; ++i) {
      auto g = std::make_unique<AccountStateMachine>();
      g->credit_genesis(alice_addr, 1000);
      auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
      nodes.push_back(std::make_unique<NetworkedNode>(kChain, vset, i,
                                                      bls(static_cast<std::uint8_t>(0x60 + i)),
                                                      std::move(ledger), /*timestamp=*/1));
    }
    for (auto [a, b] : edges) {
      nodes[a]->add_peer(b);
      nodes[b]->add_peer(a);
    }
  }

  void apply(std::uint32_t node, const NetworkedNode::Outbound& o) {
    for (const auto& r : o.relays) queue.push_back({node, r.to, r.message});
    for (const auto& t : o.timeouts) timeouts.insert({t.round, static_cast<int>(t.step), node});
  }

  void run(const std::function<bool()>& done, int budget = 200000) {
    for (std::uint32_t i = 0; i < n; ++i) apply(i, nodes[i]->start());
    for (int step = 0; step < budget; ++step) {
      if (done()) return;
      if (!queue.empty()) {
        auto [from, to, payload] = queue.front();
        queue.pop_front();
        apply(to, nodes[to]->on_gossip(from, payload));
      } else if (!timeouts.empty()) {
        auto [round, tstep, node] = *timeouts.begin();
        timeouts.erase(timeouts.begin());
        apply(node, nodes[node]->on_timeout(static_cast<Step>(tstep), round));
      } else {
        return;  // quiescent
      }
    }
  }
};
}  // namespace

int main() {
  init();
  const Hash alice_addr = crypto::address_of(view(ed(1).public_key));

  std::printf("== relay tolerance (line 0-1-2-3): all commit via hops ==\n");
  {
    // A line: node 3 is NOT directly connected to the round-0 proposer (node 0);
    // it only hears the proposal + votes relayed through 1 and 2.
    Net net(4, {{0, 1}, {1, 2}, {2, 3}}, alice_addr);
    auto all_committed = [&] {
      for (std::uint32_t i = 0; i < 4; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(all_committed);
    CHECK_MSG(all_committed(), "every node committed despite indirect connectivity");
    const Hash tip = net.nodes[0]->ledger().tip().hash();
    for (std::uint32_t i = 0; i < 4; ++i) {
      CHECK(net.nodes[i]->ledger().tip().height == 1);
      CHECK(net.nodes[i]->ledger().tip().hash() == tip);  // same block, byte-identical
    }
  }

  std::printf("== ring of 5: consensus finalizes over a cyclic topology ==\n");
  {
    Net net(5, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 0}}, alice_addr);
    auto all_committed = [&] {
      for (std::uint32_t i = 0; i < 5; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(all_committed);
    CHECK(all_committed());
    const Hash root = net.nodes[0]->ledger().tip().state_root;
    for (std::uint32_t i = 0; i < 5; ++i) CHECK(net.nodes[i]->ledger().tip().state_root == root);
  }

  std::printf("== partition tolerance: a >2/3 majority finalizes; the isolated node stalls ==\n");
  {
    // 4 validators (quorum 3). Node 3 is disconnected; 0-1-2 form a clique.
    Net net(4, {{0, 1}, {0, 2}, {1, 2}}, alice_addr);
    auto majority_committed = [&] {
      return net.nodes[0]->committed() && net.nodes[1]->committed() && net.nodes[2]->committed();
    };
    net.run(majority_committed, /*budget=*/50000);
    CHECK_MSG(majority_committed(), "the connected 3-of-4 majority committed");
    const Hash tip = net.nodes[0]->ledger().tip().hash();
    CHECK(net.nodes[1]->ledger().tip().hash() == tip);
    CHECK(net.nodes[2]->ledger().tip().hash() == tip);
    // The isolated validator never sees the proposal -> cannot finalize.
    CHECK(!net.nodes[3]->committed());
    CHECK(net.nodes[3]->ledger().tip().height == 0);
  }

  std::printf("== star topology: a hub relays the leaf validators to quorum ==\n");
  {
    // Node 0 is the hub; 1..4 connect only to 0. A vote from leaf 1 reaches leaf
    // 4 only by relaying through the hub.
    Net net(5, {{0, 1}, {0, 2}, {0, 3}, {0, 4}}, alice_addr);
    auto all_committed = [&] {
      for (std::uint32_t i = 0; i < 5; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(all_committed);
    CHECK(all_committed());
  }

  std::printf("== transactions + advance (full mesh of 4): two heights with a transfer ==\n");
  {
    Net net(4, {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}}, alice_addr);
    const auto bob = ed(2);
    const Hash bob_addr = crypto::address_of(view(bob.public_key));
    Transaction tx;
    tx.version = 1;
    tx.chain_id = kChain;
    tx.body = AccountStateMachine::encode_transfer({0, bob_addr, 250});
    sign_transaction(tx, ed(1));
    net.nodes[0]->set_transactions({tx});  // node 0 proposes at round 0
    auto all_committed = [&] {
      for (std::uint32_t i = 0; i < 4; ++i)
        if (!net.nodes[i]->committed()) return false;
      return true;
    };
    net.run(all_committed);
    CHECK(all_committed());
    for (std::uint32_t i = 0; i < 4; ++i) {
      CHECK(net.nodes[i]->ledger().tip().height == 1);
      const auto* acc = dynamic_cast<const AccountStateMachine*>(&net.nodes[i]->ledger().state());
      CHECK(acc && acc->account(bob_addr).balance == 250);
    }
    for (std::uint32_t i = 0; i < 4; ++i) net.apply(i, net.nodes[i]->advance());
    CHECK(!net.nodes[0]->committed());
    net.run(all_committed);
    CHECK(all_committed());
    for (std::uint32_t i = 0; i < 4; ++i) CHECK(net.nodes[i]->ledger().tip().height == 2);
    CHECK(net.nodes[1]->ledger().tip().encode() == net.nodes[3]->ledger().tip().encode());
    const std::string ev = net.nodes[2]->drain_events_json();
    CHECK(ev.find("\"type\":\"block_finalized\",\"height\":1") != std::string::npos);
    CHECK(ev.find("\"height\":2") != std::string::npos);
    CHECK(net.nodes[2]->drain_events_json() == "[]");
    const std::string m = net.nodes[2]->metrics_text();
    CHECK(m.find("aviary_chain_height 2") != std::string::npos);
    CHECK(m.find("aviary_gossip_received_total") != std::string::npos);
    CHECK(m.find("aviary_gossip_dropped_total") != std::string::npos);
    (void)net.nodes[0]->drain_events_json();
    int hooked = 0;
    net.nodes[0]->set_event_hook([&](const std::string&) { ++hooked; });
    for (std::uint32_t i = 0; i < 4; ++i) net.apply(i, net.nodes[i]->advance());
    net.run(all_committed);
    CHECK(hooked == 1 && net.nodes[0]->drain_events_json() == "[]");
  }

  return bctest::summary();
}
