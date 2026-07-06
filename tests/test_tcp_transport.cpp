// Real BFT consensus over real TCP sockets — first plaintext, then over the
// mutually-authenticated, PQ-hybrid SecureSession (encrypted + identity-pinned).
// Validators run one-per-thread with their own ledgers on loopback ports, dial a
// fully-connected mesh, and run the poll-reactor transport until they commit.
// Bytes cross the kernel; nothing is an in-memory relay.
#include <cstdio>
#include <memory>
#include <optional>
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
using p2p::SecureConfig;
using p2p::SecureSession;

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

// Build n whole validators (each its own ledger from the same genesis).
std::vector<std::unique_ptr<NetworkedNode>> build_nodes(std::uint32_t n, const Hash& alice_addr,
                                                        const ValidatorSet& vset) {
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
  return nodes;
}

// Run a fully-connected mesh, one reactor thread per node. secure[i] (if present)
// encrypts + authenticates node i's links. Returns per-node committed flags.
std::vector<char> run_mesh(std::vector<std::unique_ptr<NetworkedNode>>& nodes, std::uint16_t base_port,
                           const std::vector<SecureConfig>* secure) {
  const std::uint32_t n = static_cast<std::uint32_t>(nodes.size());
  std::vector<char> committed(n, 0);
  std::vector<std::thread> threads;
  for (std::uint32_t i = 0; i < n; ++i) {
    std::vector<PeerAddr> peers;
    for (std::uint32_t j = 0; j < n; ++j)
      if (j != i) peers.push_back(PeerAddr{j, "127.0.0.1", static_cast<std::uint16_t>(base_port + j)});
    const SecureConfig* sc = secure ? &(*secure)[i] : nullptr;
    threads.emplace_back([&, i, peers, sc] {
      committed[i] = p2p::run_tcp_bft_node(*nodes[i], static_cast<std::uint16_t>(base_port + i), peers,
                                           /*deadline_ms=*/8000, sc)
                         ? 1
                         : 0;
    });
  }
  for (auto& t : threads) t.join();
  return committed;
}
}  // namespace

int main() {
  init();  // crypto init once, on the main thread, before any node thread starts
  const Hash alice_addr = crypto::address_of(view(ed(1).public_key));
  const std::uint32_t n = 4;
  const ValidatorSet vset = make_vset(n);

  std::printf("== BFT over PLAINTEXT TCP: %u validators, fully-connected loopback mesh ==\n", n);
  {
    auto nodes = build_nodes(n, alice_addr, vset);
    auto committed = run_mesh(nodes, 47610, /*secure=*/nullptr);
    for (std::uint32_t i = 0; i < n; ++i) {
      CHECK_MSG(committed[i] == 1, "validator committed a block over TCP");
      CHECK(nodes[i]->ledger().tip().height == 1);
    }
    const Hash tip = nodes[0]->ledger().tip().hash();
    for (std::uint32_t i = 1; i < n; ++i)
      CHECK_MSG(nodes[i]->ledger().tip().hash() == tip, "all validators agree on the block hash");
  }

  // Long-term transport identities (independent of the BLS consensus keys). The
  // committed block is signed by the BLS keys + genesis, so it stays byte-
  // identical regardless of which ephemeral session keys encrypt the wire.
  std::vector<SecureSession::Identity> ids;
  for (std::uint32_t i = 0; i < n; ++i) ids.push_back(SecureSession::generate_identity());

  std::printf("== BFT over ENCRYPTED + AUTHENTICATED TCP (SecureSession) ==\n");
  {
    std::vector<SecureConfig> cfgs(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      cfgs[i].identity = ids[i];
      for (std::uint32_t j = 0; j < n; ++j) {
        if (j == i) continue;
        cfgs[i].peer_kem_pk[j] = ids[j].kem.public_key;
        cfgs[i].peer_sig_pk[j] = ids[j].sig.public_key;  // truthful allow-list
      }
    }
    auto nodes = build_nodes(n, alice_addr, vset);
    auto committed = run_mesh(nodes, 47630, &cfgs);
    for (std::uint32_t i = 0; i < n; ++i) {
      CHECK_MSG(committed[i] == 1, "validator committed over the encrypted channel");
      CHECK(nodes[i]->ledger().tip().height == 1);
    }
    const Hash tip = nodes[0]->ledger().tip().hash();
    for (std::uint32_t i = 1; i < n; ++i)
      CHECK_MSG(nodes[i]->ledger().tip().hash() == tip, "encrypted validators agree on the block hash");
  }

  std::printf("== allow-list rejection: a mis-pinned identity is excluded; the honest quorum still commits ==\n");
  {
    // Everyone holds the truthful keys EXCEPT: nodes 0,1,2 have the wrong signing
    // key pinned for node 3. When they dial node 3, its real (authenticated)
    // identity won't match what they expect → they drop the link. Node 3 is thus
    // excluded, but 0-1-2 are a quorum (3 of 4) and finalize without it.
    const SecureSession::Identity wrong = SecureSession::generate_identity();
    std::vector<SecureConfig> cfgs(n);
    for (std::uint32_t i = 0; i < n; ++i) {
      cfgs[i].identity = ids[i];
      for (std::uint32_t j = 0; j < n; ++j) {
        if (j == i) continue;
        cfgs[i].peer_kem_pk[j] = ids[j].kem.public_key;
        const bool mispin = (i < 3 && j == 3);
        cfgs[i].peer_sig_pk[j] = mispin ? wrong.sig.public_key : ids[j].sig.public_key;
      }
    }
    auto nodes = build_nodes(n, alice_addr, vset);
    auto committed = run_mesh(nodes, 47650, &cfgs);
    for (std::uint32_t i = 0; i < 3; ++i)
      CHECK_MSG(committed[i] == 1, "the honest quorum finalized without the mis-pinned peer");
    const Hash tip = nodes[0]->ledger().tip().hash();
    CHECK(nodes[1]->ledger().tip().hash() == tip);
    CHECK(nodes[2]->ledger().tip().hash() == tip);
    CHECK_MSG(committed[3] == 0, "the mis-pinned validator was excluded and did not commit");
  }

  return bctest::summary();
}
