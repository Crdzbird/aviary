// Gossip overlay: a message originated at one node floods to every node in an
// arbitrary topology exactly once, with content-hash dedup stopping loops.
#include <deque>
#include <map>
#include <vector>

#include "blockchainer/gossip.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using p2p::GossipOverlay;

namespace {
Bytes msg(const char* s) {
  Bytes b;
  for (; *s; ++s) b.push_back(static_cast<std::uint8_t>(*s));
  return b;
}

// A network of overlays connected by [edges] (undirected). Delivers relays until
// quiescent, counting how many nodes accepted (first-saw) each message and how
// many deliveries were deduped.
struct Net {
  std::uint32_t n;
  std::vector<GossipOverlay> nodes;
  std::deque<GossipOverlay::Relay> queue;  // pending deliveries carry their target
  std::vector<std::uint32_t> queue_from;   // parallel: sender per queued relay
  int accepts = 0;
  int dedups = 0;

  Net(std::uint32_t n_, const std::vector<std::pair<std::uint32_t, std::uint32_t>>& edges) : n(n_) {
    for (std::uint32_t i = 0; i < n; ++i) nodes.emplace_back(i);
    for (auto [a, b] : edges) {
      nodes[a].add_peer(b);
      nodes[b].add_peer(a);
    }
  }

  void enqueue(std::uint32_t from, const std::vector<GossipOverlay::Relay>& relays) {
    for (const auto& r : relays) {
      queue.push_back(r);
      queue_from.push_back(from);
    }
  }

  void run(std::uint32_t origin, const Bytes& m) {
    enqueue(origin, nodes[origin].originate(m));
    while (!queue.empty()) {
      GossipOverlay::Relay r = queue.front();
      std::uint32_t from = queue_from.front();
      queue.pop_front();
      queue_from.erase(queue_from.begin());
      const bool was_new = !nodes[r.to].seen(r.message);
      auto out = nodes[r.to].on_receive(from, r.message);
      if (was_new) {
        ++accepts;
        enqueue(r.to, out);
      } else {
        ++dedups;
      }
    }
  }
};
}  // namespace

int main() {
  init();

  std::printf("== single overlay: dedup, originate floods to peers ==\n");
  {
    GossipOverlay g(0);
    g.add_peer(1);
    g.add_peer(2);
    g.add_peer(0);  // self ignored
    CHECK(g.peers().size() == 2);
    auto relays = g.originate(msg("hello"));
    CHECK(relays.size() == 2);          // to peers 1 and 2
    CHECK(g.seen(msg("hello")));
    // A message received that we already originated is deduped (no relays).
    CHECK(g.on_receive(1, msg("hello")).empty());
    // A new message from peer 1 relays only to peer 2 (not back to 1).
    auto r2 = g.on_receive(1, msg("world"));
    CHECK(r2.size() == 1 && r2[0].to == 2);
  }

  std::printf("== ring of 6: one message reaches all, loop-free ==\n");
  {
    // 0-1-2-3-4-5-0 ring (each node has 2 peers; lots of loop potential).
    Net net(6, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 0}});
    net.run(0, msg("block-42"));
    // Every non-origin node accepted exactly once → 5 accepts.
    CHECK(net.accepts == 5);
    for (std::uint32_t i = 0; i < 6; ++i) CHECK(net.nodes[i].seen(msg("block-42")));
    // Extra deliveries (the ring's redundant paths) were all deduped, not looped.
    CHECK(net.dedups > 0);
  }

  std::printf("== partial mesh of 7: full propagation regardless of topology ==\n");
  {
    // A tree + a couple of cross links (redundant paths).
    Net net(7, {{0, 1}, {0, 2}, {1, 3}, {1, 4}, {2, 5}, {2, 6}, {3, 4}, {5, 6}});
    net.run(3, msg("tx-abc"));  // originate from a leaf-ish node
    CHECK(net.accepts == 6);    // all 6 others
    for (std::uint32_t i = 0; i < 7; ++i) CHECK(net.nodes[i].seen(msg("tx-abc")));
  }

  std::printf("== a fully-connected clique still delivers once per node ==\n");
  {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> edges;
    for (std::uint32_t a = 0; a < 5; ++a)
      for (std::uint32_t b = a + 1; b < 5; ++b) edges.push_back({a, b});
    Net net(5, edges);
    net.run(0, msg("clique"));
    CHECK(net.accepts == 4);  // 4 non-origin nodes each accept exactly once
    for (std::uint32_t i = 0; i < 5; ++i) CHECK(net.nodes[i].seen(msg("clique")));
  }

  return bctest::summary();
}
