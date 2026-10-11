#include <memory>
#include "aviary/node/demo_node.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "aviary/observability/events.hpp"

namespace aviary::node {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

}  // namespace

DemoNode::DemoNode(std::uint64_t chain_id) : chain_id_(chain_id) {
  // Deterministic validator identity for the demo (one local validator).
  codec::CodecWriter w;
  w.write_u64(chain_id);
  Hash ikm = crypto::hash_tagged(crypto::Domain::kHeader, w.take());
  validator_ = crypto::bls_keygen_from_ikm(view(ikm));
  genesis_ = std::make_unique<AccountStateMachine>();
}

void DemoNode::ensure_started() {
  if (engine_) return;
  start_engine(std::make_unique<Ledger>(ChainSpec{chain_id_, StateModel::kAccount},
                                        std::unique_ptr<StateMachine>(genesis_.release())));
}

void DemoNode::start_engine(std::unique_ptr<Ledger> ledger) {
  consensus::ValidatorSet vset(
      std::vector<consensus::Validator>{{validator_.public_key, 1}});  // quorum 1
  engine_ = std::make_unique<consensus::BftEngine>(std::move(vset), std::move(ledger));
  engine_->set_observability(&bus_, &metrics_);
  mempool_.set_event_bus(&bus_);

}

void DemoNode::set_event_hook(EventHook hook) { log_.set_hook(std::move(hook)); }

const AccountStateMachine& DemoNode::read_state() const {
  if (engine_) {
    return dynamic_cast<const AccountStateMachine&>(engine_->ledger().state());
  }
  return *genesis_;  // pre-start: read the (still-mutable) genesis state
}

void DemoNode::credit(const Hash& address, codec::u128 amount) {
  if (engine_) throw GenesisSealedError();
  genesis_->credit_genesis(address, amount);
  credits_.emplace_back(address, amount);
}

crypto::KeyPair DemoNode::new_account() { return crypto::ed25519_keygen(); }

crypto::KeyPair DemoNode::new_account_hybrid() { return crypto::hybrid_sig_keygen(); }

Hash DemoNode::submit_transfer_hybrid(codec::ByteView secret, codec::ByteView public_key,
                                      const Hash& to, codec::u128 amount, std::uint64_t nonce) {
  ensure_started();
  crypto::KeyPair kp;
  kp.secret_key = Bytes(secret.begin(), secret.end());
  kp.public_key = Bytes(public_key.begin(), public_key.end());

  Transaction tx;
  tx.version = 2;  // extended envelope
  tx.sig_algo = static_cast<std::uint8_t>(SigAlgo::kHybridEd25519MlDsa);
  tx.chain_id = chain_id_;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, kp);
  Hash id = tx.txid();
  if (mempool_.add(tx, chain_id_) != AddResult::kAdded) {
    throw std::runtime_error("demo_node: hybrid transfer rejected by mempool");
  }
  return id;
}

Hash DemoNode::submit_transfer(codec::ByteView from_secret, const Hash& to,
                               codec::u128 amount, std::uint64_t nonce) {
  ensure_started();
  if (from_secret.size() != 64) throw std::runtime_error("demo_node: ed25519 secret must be 64 bytes");
  // libsodium layout: secret = seed(32) ‖ public(32).
  crypto::KeyPair kp;
  kp.secret_key = Bytes(from_secret.begin(), from_secret.end());
  kp.public_key = Bytes(from_secret.begin() + 32, from_secret.end());

  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain_id_;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, kp);
  Hash id = tx.txid();
  if (mempool_.add(tx, chain_id_) != AddResult::kAdded) {
    throw std::runtime_error("demo_node: transfer rejected by mempool");
  }
  return id;
}

Hash DemoNode::submit_transfer_fee(codec::ByteView from_secret, const Hash& to,
                                   codec::u128 amount, std::uint64_t nonce, codec::u128 tip) {
  ensure_started();
  if (from_secret.size() != 64) throw std::runtime_error("demo_node: ed25519 secret must be 64 bytes");
  crypto::KeyPair kp;
  kp.secret_key = Bytes(from_secret.begin(), from_secret.end());
  kp.public_key = Bytes(from_secret.begin() + 32, from_secret.end());

  Transaction tx;
  tx.version = 2;  // extended envelope carries the fee
  tx.sig_algo = static_cast<std::uint8_t>(SigAlgo::kEd25519);
  tx.fee = tip;
  tx.chain_id = chain_id_;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amount});
  sign_transaction(tx, kp);
  Hash id = tx.txid();
  if (mempool_.add(tx, chain_id_) != AddResult::kAdded) {
    throw std::runtime_error("demo_node: fee transfer rejected by mempool");
  }
  return id;
}

Hash DemoNode::submit(const Transaction& tx) {
  ensure_started();
  if (tx.chain_id != chain_id_) throw std::runtime_error("demo_node: transaction is for another chain");
  Hash id = tx.txid();
  if (mempool_.add(tx, chain_id_) != AddResult::kAdded) {
    throw std::runtime_error("demo_node: transaction rejected by mempool");
  }
  return id;
}

Hash DemoNode::submit_encoded(codec::ByteView tx_bytes) {
  return submit(Transaction::decode_all(tx_bytes));
}

std::uint64_t DemoNode::produce_block(std::uint64_t timestamp) {
  ensure_started();
  auto txs = mempool_.select(engine_->ledger().state(), 1000);
  Block blk = engine_->propose(std::move(txs), timestamp);
  auto cert = engine_->try_commit(blk, {engine_->precommit(blk, 0, validator_)});
  if (!cert) throw std::runtime_error("demo_node: failed to reach quorum");
  if (auto e = engine_->finalize(blk, *cert); !e.ok) {
    throw std::runtime_error("demo_node: finalize failed: " + e.error);
  }
  archive_.put(blk, *cert);
  std::vector<Hash> ids;
  for (const auto& t : blk.transactions) ids.push_back(t.txid());
  mempool_.remove(ids);
  return engine_->height();
}

namespace {
constexpr std::uint8_t kSnapshotMagic[4] = {'A', 'V', 'S', 'N'};
constexpr std::uint8_t kSnapshotVersion = 2;
}  // namespace

Bytes DemoNode::snapshot() const {
  codec::CodecWriter w;
  for (std::uint8_t b : kSnapshotMagic) w.write_u8(b);
  w.write_u8(kSnapshotVersion);
  w.write_u64(chain_id_);
  w.write_u32(static_cast<std::uint32_t>(credits_.size()));
  for (const auto& [address, amount] : credits_) {
    w.write_fixed_bytes(view(address), address.size());
    w.write_u128(amount);
  }
  const Bytes archive = archive_.serialize();
  w.write_bytes(view(archive));
  const Bytes state = engine_ ? engine_->ledger().snapshot() : Bytes{};
  w.write_bytes(view(state));
  return w.take();
}

std::unique_ptr<DemoNode> DemoNode::restore(codec::ByteView bytes, bool replay) {
  codec::CodecReader r(bytes);
  bool versioned = bytes.size() >= 5;
  for (std::size_t i = 0; versioned && i < 4; ++i) versioned = bytes[i] == kSnapshotMagic[i];
  if (versioned) {
    for (int i = 0; i < 4; ++i) (void)r.read_u8();
    if (r.read_u8() != kSnapshotVersion) throw std::runtime_error("demo_node: unknown snapshot version");
  }
  auto node = std::make_unique<DemoNode>(r.read_u64());
  const std::uint32_t credits = r.read_u32();
  for (std::uint32_t i = 0; i < credits; ++i) {
    const Bytes raw = r.read_fixed_bytes(sizeof(Hash));
    Hash address{};
    std::copy(raw.begin(), raw.end(), address.begin());
    node->credit(address, r.read_u128());
  }
  const Bytes archive_bytes = r.read_bytes();
  Bytes state;
  if (versioned) state = r.read_bytes();
  if (r.remaining() != 0) throw std::runtime_error("demo_node: trailing bytes in snapshot");
  BlockStore archive = BlockStore::load(view(archive_bytes));
  if (archive.empty()) return node;
  if (!replay && !state.empty()) {
    node->start_engine(Ledger::restore(ChainSpec{node->chain_id_, StateModel::kAccount}, view(state)));
    if (node->engine_->height() != archive.tip_height()) {
      throw std::runtime_error("demo_node: snapshot state and archive disagree on the height");
    }
    node->engine_->resume(archive.by_height(archive.tip_height()).commit);
    node->archive_ = std::move(archive);
    node->genesis_.reset();
    node->log_.clear();
    return node;
  }
  node->ensure_started();
  for (std::uint64_t h = 1; h <= archive.tip_height(); ++h) {
    const StoredBlock& sb = archive.by_height(h);
    if (auto e = node->engine_->finalize(sb.block, sb.commit); !e.ok) {
      throw std::runtime_error("demo_node: replay failed at height " + std::to_string(h) + ": " + e.error);
    }
    node->archive_.put(sb.block, sb.commit);
  }
  node->log_.clear();
  return node;
}

std::uint64_t DemoNode::height() const { return engine_ ? engine_->height() : 0; }

codec::u128 DemoNode::balance(const Hash& address) const {
  return read_state().account(address).balance;
}
std::uint64_t DemoNode::nonce(const Hash& address) const {
  return read_state().account(address).nonce;
}
std::uint64_t DemoNode::next_nonce(const Hash& address) const {
  const std::uint64_t committed = nonce(address);
  const std::optional<std::uint64_t> pending = mempool_.highest_pending_nonce(address);
  return pending && *pending + 1 > committed ? *pending + 1 : committed;
}
Hash DemoNode::state_root() const { return read_state().state_root(); }

std::string DemoNode::drain_events_json() { return log_.drain_json(); }

std::string DemoNode::metrics_text() {
  // Non-sealing: a metrics scrape must never start the engine. Engine counters
  // appear once the engine registers them (on first transact/produce).
  metrics_.gauge("aviary_mempool_size", "pending transactions")
      .set(static_cast<std::int64_t>(mempool_.size()));
  return metrics_.prometheus_text();
}

StateProof DemoNode::prove_account(const Hash& address) const {
  return read_state().prove_account(address);
}

}  // namespace aviary::node
