#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/assurance.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/genesis.hpp"
#include "aviary/ledger.hpp"
#include "aviary/networked_node.hpp"
#include "aviary/node/demo_node.hpp"
#include "aviary/proofs.hpp"
#include "aviary/record_state.hpp"
#include "aviary/version.hpp"

using namespace aviary;
using clk = std::chrono::steady_clock;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

double ms_since(clk::time_point t) { return std::chrono::duration<double, std::milli>(clk::now() - t).count(); }

void row(const char* what, double ms, std::size_t n, const char* unit) {
  std::printf("| %s | %zu | %.1f ms | %.0f %s/s |\n", what, n, ms, n / (ms / 1000.0), unit);
}

crypto::KeyPair bls(std::uint8_t seed) {
  Bytes ikm(32, seed);
  return crypto::bls_keygen_from_ikm(view(ikm));
}
}

int main() {
  init();
  constexpr std::size_t kAccounts = 1000;
  constexpr int kBlocks = 5;
  std::printf("aviary %s, single thread\n\n", version().c_str());
  std::printf("| Phase | Items | Time | Rate |\n|---|---|---|---|\n");

  node::DemoNode node(1);
  std::vector<crypto::KeyPair> accounts;
  for (std::size_t i = 0; i < kAccounts; ++i) {
    accounts.push_back(node.new_account());
    node.credit(crypto::address_of(view(accounts.back().public_key)), 1000000);
  }
  const Hash sink = crypto::address_of(view(accounts[0].public_key));
  double admit_ms = 0, block_ms = 0;
  for (int b = 0; b < kBlocks; ++b) {
    auto t = clk::now();
    for (std::size_t i = 0; i < kAccounts; ++i) {
      node.submit_transfer(view(accounts[i].secret_key), sink, 1, static_cast<std::uint64_t>(b));
    }
    admit_ms += ms_since(t);
    t = clk::now();
    node.produce_block(static_cast<std::uint64_t>(b + 2));
    block_ms += ms_since(t);
  }
  const std::size_t txs = kAccounts * kBlocks;
  row("Ed25519 transfer, sign and admit", admit_ms, txs, "tx");
  row("BFT block of 1,000 transfers, build, validate, apply, certify", block_ms, txs, "tx");
  row("End to end, admit and finalize", admit_ms + block_ms, txs, "tx");

  const Bytes snap = node.snapshot();
  auto t = clk::now();
  auto replayed = node::DemoNode::restore(view(snap), true);
  row("Follower replay, every certificate and signature verified", ms_since(t), txs, "tx");
  t = clk::now();
  auto fast = node::DemoNode::restore(view(snap), false);
  std::printf("| Restore from snapshot, %zu blocks | 1 | %.1f ms | |\n", static_cast<std::size_t>(kBlocks), ms_since(t));

  const Hash probe = crypto::address_of(view(accounts[kAccounts / 2].public_key));
  constexpr int kProofs = 200;
  t = clk::now();
  StateProof proof;
  for (int i = 0; i < kProofs; ++i) proof = node.prove_account(probe);
  row("State proof over 1,000 accounts, prove", ms_since(t), kProofs, "proof");
  const Hash root = node.state_root();
  t = clk::now();
  bool ok = true;
  for (int i = 0; i < kProofs; ++i) ok = ok && verify_state_proof(root, proof);
  row("State proof, verify", ms_since(t), kProofs, "proof");
  std::printf("| State proof size | 1 | %zu bytes | |\n", proof.encode().size());

  node::DemoNode pq(2);
  constexpr std::size_t kHybrid = 200;
  std::vector<crypto::KeyPair> hybrid;
  for (std::size_t i = 0; i < kHybrid; ++i) {
    hybrid.push_back(pq.new_account_hybrid());
    pq.credit(crypto::address_of(view(hybrid.back().public_key)), 1000);
  }
  t = clk::now();
  for (std::size_t i = 0; i < kHybrid; ++i) {
    pq.submit_transfer_hybrid(view(hybrid[i].secret_key), view(hybrid[i].public_key), sink, 1, 0);
  }
  pq.produce_block(2);
  row("Hybrid Ed25519 + ML-DSA-65 transfer, admit and finalize", ms_since(t), kHybrid, "tx");

  GenesisSpec g;
  g.chain_id = 3;
  g.timestamp = 1;
  g.state_model = StateModel::kRecord;
  AssuranceProfile::standard().apply_to(g);
  const crypto::KeyPair v = bls(0x60);
  g.validators.push_back(consensus::Validator{v.public_key, 1});
  ChainBlueprint bp;
  bp.consensus_id = "bft";
  bp.state_model_id = "record";
  g.chain_blueprint = bp.encode();
  auto chain = node::ChainAssembler::assemble(g, 0, v);
  auto* bft = dynamic_cast<consensus::BftEngine*>(chain.engine.get());
  t = clk::now();
  std::vector<Transaction> writes;
  for (std::size_t i = 0; i < kAccounts; ++i) {
    Hash key{};
    key[0] = static_cast<std::uint8_t>(i);
    key[1] = static_cast<std::uint8_t>(i >> 8);
    Transaction tx;
    tx.chain_id = 3;
    tx.body = RecordStateMachine::encode_write({0, key, RecordStateMachine::kPut, Bytes(64, 0x41)});
    sign_transaction(tx, accounts[i]);
    writes.push_back(std::move(tx));
  }
  Block blk = chain.engine->propose(writes, 2);
  const auto vote = bft->precommit(blk, 0, v);
  const auto cert = bft->try_commit(blk, {vote});
  const bool applied = chain.engine->submit(blk, view(cert->encode())).ok;
  row("Record writes of 64 bytes, sign and finalize in one block", ms_since(t), kAccounts, "write");

  constexpr std::uint32_t kValidators = 4;
  constexpr int kHeights = 50;
  std::vector<consensus::Validator> vs;
  for (std::uint32_t i = 0; i < kValidators; ++i) vs.push_back(consensus::Validator{bls(static_cast<std::uint8_t>(0x70 + i)).public_key, 1});
  consensus::ValidatorSet vset(vs);
  std::vector<std::unique_ptr<p2p::NetworkedNode>> mesh;
  for (std::uint32_t i = 0; i < kValidators; ++i) {
    auto ledger = std::make_unique<Ledger>(ChainSpec{4, StateModel::kAccount}, std::make_unique<AccountStateMachine>());
    mesh.push_back(std::make_unique<p2p::NetworkedNode>(4, vset, i, bls(static_cast<std::uint8_t>(0x70 + i)), std::move(ledger), 1));
  }
  for (std::uint32_t a = 0; a < kValidators; ++a) {
    for (std::uint32_t b = a + 1; b < kValidators; ++b) {
      mesh[a]->add_peer(b);
      mesh[b]->add_peer(a);
    }
  }
  std::deque<std::tuple<std::uint32_t, std::uint32_t, Bytes>> queue;
  auto apply = [&](std::uint32_t from, const p2p::NetworkedNode::Outbound& o) {
    for (const auto& r : o.relays) queue.emplace_back(from, r.to, r.message);
  };
  std::size_t delivered = 0;
  auto settle = [&] {
    while (!queue.empty()) {
      ++delivered;
      auto [from, to, payload] = queue.front();
      queue.pop_front();
      apply(to, mesh[to]->on_gossip(from, payload));
    }
  };
  t = clk::now();
  for (std::uint32_t i = 0; i < kValidators; ++i) apply(i, mesh[i]->start());
  settle();
  for (int h = 1; h < kHeights; ++h) {
    for (std::uint32_t i = 0; i < kValidators; ++i) apply(i, mesh[i]->advance());
    settle();
  }
  const bool reached = mesh[0]->ledger().tip().height == static_cast<std::uint64_t>(kHeights);
  row("4 validator BFT height, in-process full mesh", ms_since(t), kHeights, "height");
  std::printf("| Gossip deliveries per height, 4 validators | %zu | | |\n", delivered / kHeights);

  std::printf("\nchecks: proofs %s, replay %s, fast restore %s, records %s, mesh %s\n", ok ? "ok" : "FAIL",
              replayed && replayed->height() == node.height() ? "ok" : "FAIL",
              fast && fast->state_root() == node.state_root() ? "ok" : "FAIL", applied ? "ok" : "FAIL",
              reached ? "ok" : "FAIL");
  return ok && applied && reached ? 0 : 1;
}
