// Gossip overlay: a message originated at one node floods to every node in an
// arbitrary topology exactly once, with content-hash dedup stopping loops.
#include <deque>
#include <map>
#include <vector>

#include "aviary/credential_gate.hpp"
#include "aviary/gossip.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
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

  std::printf("== bounded seen-set: the window evicts oldest, capping memory ==\n");
  {
    GossipOverlay g(0, GossipOverlay::Config{/*seen_capacity=*/4, /*ban_threshold=*/50});
    g.add_peer(1);
    for (int i = 0; i < 100; ++i) {
      Bytes m = msg("m");
      m.push_back(static_cast<std::uint8_t>(i));
      g.originate(m);
    }
    CHECK_MSG(g.seen_count() == 4, "seen-set never exceeds capacity");
    // The most-recent ids are still remembered; long-evicted ones are forgotten.
    Bytes recent = msg("m");
    recent.push_back(99);
    CHECK(g.seen(recent));
    Bytes old = msg("m");
    old.push_back(0);
    CHECK(!g.seen(old));  // aged out of the window
  }

  std::printf("== peer scoring + ban: a penalized peer is cut off ==\n");
  {
    GossipOverlay g(0, GossipOverlay::Config{/*seen_capacity=*/1024, /*ban_threshold=*/3});
    g.add_peer(1);
    g.add_peer(2);
    g.add_peer(3);
    CHECK(!g.banned(1));
    CHECK(!g.penalize(1));  // score 1
    CHECK(!g.penalize(1));  // score 2
    CHECK_MSG(g.penalize(1), "crossing the threshold bans the peer");
    CHECK(g.banned(1));
    CHECK(g.score(1) == 3);

    // A banned peer is no longer relayed to, and its messages are ignored.
    auto relays = g.originate(msg("after-ban"));
    CHECK_MSG(relays.size() == 2, "originate skips the banned peer");  // to 2 and 3, not 1
    for (const auto& r : relays) CHECK(r.to != 1);
    CHECK_MSG(g.on_receive(1, msg("from-banned")).empty(), "messages from a banned peer are dropped");
    CHECK(!g.seen(msg("from-banned")));  // never even entered the seen-set
    // An honest peer is unaffected: its message relays onward (to 3, not back to 2).
    CHECK(!g.banned(2));
    auto fwd = g.on_receive(2, msg("from-honest"));
    CHECK_MSG(fwd.size() == 1 && fwd[0].to == 3, "an honest peer's message still floods onward");
  }

  // ── Membership admission gate (CredentialGate, Option A) ─────────────────────
  // A permissioned mesh admits a peer only behind a valid "gossip"-scope
  // presentation. The plain add_peer() is the trusted path and stays ungated.
  {
    auto viewb = [](const Bytes& b) { return codec::ByteView(b.data(), b.size()); };
    auto issuer = crypto::bbs_keygen(viewb(Bytes(32, 0x5A)));
    consensus::CredentialPolicy policy;
    policy.chain_id = 9;
    policy.issuer_public_key = issuer.public_key;
    policy.header = msg("gossip-member");
    policy.min_tier = 1;
    consensus::CredentialGate gate(policy);

    Bytes member_secret(32, 0x22);
    auto cred = consensus::issue_credential(policy, viewb(issuer.secret_key), viewb(member_secret),
                                            /*tier=*/2, /*not_after=*/4102444800ULL,
                                            /*issuer_epoch=*/1);
    const std::uint64_t now = 1700000000ULL;
    Bytes challenge(32, 0x7E);

    GossipOverlay g(0);
    g.set_credential_gate(&gate);

    // Without a valid presentation, the peer is not admitted to the mesh.
    CHECK(!g.admit_peer(1, 3, viewb(challenge), viewb(Bytes{9, 9}), now));
    CHECK(g.peers().count(1) == 0);

    // A valid "gossip" presentation for (epoch 3, this challenge) admits it.
    Bytes gp = consensus::CredentialGate::present(policy, cred, 3, "gossip", viewb(challenge));
    CHECK(g.admit_peer(1, 3, viewb(challenge), viewb(gp), now));
    CHECK(g.peers().count(1) == 1);

    // A "submit"-scope proof does not admit a gossip peer (scope binding).
    Bytes sp = consensus::CredentialGate::present(policy, cred, 3, "submit", viewb(challenge));
    CHECK(!g.admit_peer(2, 3, viewb(challenge), viewb(sp), now));
    CHECK(g.peers().count(2) == 0);

    // The plain add_peer() is the trusted path — never gated, even with a gate set.
    g.add_peer(2);
    CHECK(g.peers().count(2) == 1);

    // An ungated overlay admits any peer (permissionless).
    GossipOverlay open(0);
    CHECK(open.admit_peer(5, 0, viewb(challenge), viewb(Bytes{}), now));
    CHECK(open.peers().count(5) == 1);
  }

  return bctest::summary();
}
