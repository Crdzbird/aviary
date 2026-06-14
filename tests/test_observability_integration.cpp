// Observability wired into a running chain: events fire and metrics update as
// transactions are admitted and blocks finalize.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/mempool.hpp"
#include "blockchainer/observability/event_bus.hpp"
#include "blockchainer/observability/events.hpp"
#include "blockchainer/observability/metrics.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;
namespace obs = blockchainer::observe;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
constexpr std::uint64_t kChain = 7;
Transaction transfer(const crypto::KeyPair& f, const Hash& to, std::uint64_t n, codec::u128 a) {
  Transaction tx; tx.version = 1; tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({n, to, a});
  sign_transaction(tx, f);
  return tx;
}
bool contains(const std::string& h, const std::string& n) { return h.find(n) != std::string::npos; }
}  // namespace

int main() {
  init();

  auto vkey = bls(100);
  ValidatorSet vset(std::vector<Validator>{{vkey.public_key, 1}});
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  obs::EventBus bus;
  obs::MetricsRegistry metrics;

  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
  BftEngine engine(vset, std::move(ledger));
  engine.set_observability(&bus, &metrics);
  Mempool mp;
  mp.set_event_bus(&bus);

  // Subscribers (the streaming API).
  std::vector<std::uint64_t> finalized_heights;
  std::vector<std::uint64_t> finalized_txcounts;
  int admitted = 0, removed = 0, rejected = 0;
  bus.subscribe<obs::BlockFinalized>([&](const obs::BlockFinalized& e) {
    finalized_heights.push_back(e.height);
    finalized_txcounts.push_back(e.tx_count);
  });
  bus.subscribe<obs::TxAdmitted>([&](const obs::TxAdmitted&) { ++admitted; });
  bus.subscribe<obs::TxRemoved>([&](const obs::TxRemoved&) { ++removed; });
  bus.subscribe<obs::BlockRejected>([&](const obs::BlockRejected&) { ++rejected; });

  std::printf("== admitting txs fires TxAdmitted ==\n");
  mp.add(transfer(alice, bob_addr, 0, 100), kChain);
  mp.add(transfer(alice, bob_addr, 1, 50), kChain);
  CHECK(admitted == 2);

  std::printf("== finalizing a block fires BlockFinalized + updates metrics ==\n");
  {
    auto txs = mp.select(engine.ledger().state(), 100);
    Block blk = engine.propose(txs, 1);
    auto cert = engine.try_commit(blk, {engine.precommit(blk, 0, vkey)});
    CHECK(engine.finalize(blk, *cert).ok);
    std::vector<Hash> ids;
    for (const auto& t : blk.transactions) ids.push_back(t.txid());
    mp.remove(ids);
  }
  CHECK(finalized_heights.size() == 1);
  CHECK(finalized_heights[0] == 1);
  CHECK(finalized_txcounts[0] == 2);  // both alice txs included
  CHECK(removed == 2);

  CHECK(metrics.gauge("blockchainer_chain_height").value() == 1);
  CHECK(metrics.counter("blockchainer_blocks_finalized_total").value() == 1);
  CHECK(metrics.counter("blockchainer_transactions_total").value() == 2);

  std::printf("== a second (empty) block advances height, not tx count ==\n");
  {
    Block blk = engine.propose({}, 2);
    auto cert = engine.try_commit(blk, {engine.precommit(blk, 0, vkey)});
    CHECK(engine.finalize(blk, *cert).ok);
  }
  CHECK(finalized_heights.size() == 2);
  CHECK(metrics.gauge("blockchainer_chain_height").value() == 2);
  CHECK(metrics.counter("blockchainer_transactions_total").value() == 2);  // unchanged

  std::printf("== a rejected block fires BlockRejected ==\n");
  {
    Block blk = engine.propose({}, 3);
    auto cert = engine.try_commit(blk, {engine.precommit(blk, 0, vkey)});
    Block tampered = blk;
    tampered.header.state_root[0] ^= 0x01;  // cert no longer matches → rejected
    CHECK(!engine.finalize(tampered, *cert).ok);
  }
  CHECK(rejected == 1);

  std::printf("== prometheus exposition reflects chain state ==\n");
  {
    const std::string text = metrics.prometheus_text();
    CHECK(contains(text, "blockchainer_chain_height 2"));
    CHECK(contains(text, "blockchainer_blocks_finalized_total 2"));
    CHECK(contains(text, "blockchainer_transactions_total 2"));
  }

  return bctest::summary();
}
