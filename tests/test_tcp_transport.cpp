// The final mile: real BFT consensus over real TCP sockets. Four validators,
// each in its own thread with its own ledger, listen on loopback ports, dial a
// fully-connected mesh, and run the poll-reactor transport (run_tcp_bft_node)
// until they commit. No in-memory relay — bytes cross the kernel. This proves
// the pure NetworkedNode composes with an actual socket/event-loop transport.
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/networked_node.hpp"
#include "blockchainer/tcp_transport.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;
using p2p::NetworkedNode;
using p2p::PeerAddr;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 42;
constexpr std::uint16_t kBasePort = 47610;

ValidatorSet make_vset(std::uint32_t n) {
  std::vector<Validator> vs;
  for (std::uint32_t i = 0; i < n; ++i) vs.push_back(Validator{bls(static_cast<std::uint8_t>(0x60 + i)).public_key, 1});
  return ValidatorSet(std::move(vs));
}
}  // namespace

int main() {
  init();  // crypto init once, on the main thread, before any node thread starts
  const Hash alice_addr = crypto::address_of(view(ed(1).public_key));
  const std::uint32_t n = 4;
  const ValidatorSet vset = make_vset(n);

  std::printf("== BFT over real TCP: %u validators, fully-connected loopback mesh ==\n", n);

  // Build one whole validator per index (own ledger seeded from the same genesis).
  std::vector<std::unique_ptr<NetworkedNode>> nodes;
  for (std::uint32_t i = 0; i < n; ++i) {
    auto g = std::make_unique<AccountStateMachine>();
    g->credit_genesis(alice_addr, 1000);
    auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
    nodes.push_back(std::make_unique<NetworkedNode>(kChain, vset, i, bls(static_cast<std::uint8_t>(0x60 + i)),
                                                    std::move(ledger), /*timestamp=*/1));
    for (std::uint32_t j = 0; j < n; ++j)
      if (j != i) nodes[i]->add_peer(j);
  }

  // Each validator runs the socket reactor on its own thread.
  std::vector<std::thread> threads;
  std::vector<char> committed(n, 0);
  for (std::uint32_t i = 0; i < n; ++i) {
    std::vector<PeerAddr> peers;
    for (std::uint32_t j = 0; j < n; ++j)
      if (j != i) peers.push_back(PeerAddr{j, "127.0.0.1", static_cast<std::uint16_t>(kBasePort + j)});
    threads.emplace_back([&, i, peers] {
      committed[i] = p2p::run_tcp_bft_node(*nodes[i], static_cast<std::uint16_t>(kBasePort + i), peers,
                                           /*deadline_ms=*/8000)
                         ? 1
                         : 0;
    });
  }
  for (auto& t : threads) t.join();

  for (std::uint32_t i = 0; i < n; ++i) {
    CHECK_MSG(committed[i] == 1, "validator committed a block over TCP");
    CHECK(nodes[i]->ledger().tip().height == 1);
  }
  // Safety: every honest validator finalized the SAME block, byte-identical.
  const Hash tip = nodes[0]->ledger().tip().hash();
  for (std::uint32_t i = 1; i < n; ++i)
    CHECK_MSG(nodes[i]->ledger().tip().hash() == tip, "all validators agree on the block hash");

  return bctest::summary();
}
