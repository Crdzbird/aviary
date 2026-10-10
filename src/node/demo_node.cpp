#include "aviary/node/demo_node.hpp"

#include <sstream>
#include <stdexcept>

#include "aviary/observability/events.hpp"

namespace aviary::node {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

std::string hex(const Hash& h) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s;
  for (std::uint8_t b : h) { s.push_back(k[b >> 4]); s.push_back(k[b & 0xF]); }
  return s;
}
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
  consensus::ValidatorSet vset(
      std::vector<consensus::Validator>{{validator_.public_key, 1}});  // quorum 1
  auto ledger = std::make_unique<Ledger>(
      ChainSpec{chain_id_, StateModel::kAccount},
      std::unique_ptr<StateMachine>(genesis_.release()));
  engine_ = std::make_unique<consensus::BftEngine>(std::move(vset), std::move(ledger));
  engine_->set_observability(&bus_, &metrics_);
  mempool_.set_event_bus(&bus_);

  // Collect chain events as JSON fragments (the streaming surface).
  bus_.subscribe<observe::BlockFinalized>([this](const observe::BlockFinalized& e) {
    std::ostringstream os;
    os << "{\"type\":\"block_finalized\",\"height\":" << e.height
       << ",\"tx_count\":" << e.tx_count << ",\"state_root\":\"" << hex(e.state_root) << "\"}";
    events_.push_back(os.str());
  });
  bus_.subscribe<observe::TxAdmitted>([this](const observe::TxAdmitted& e) {
    std::ostringstream os;
    os << "{\"type\":\"tx_admitted\",\"txid\":\"" << hex(e.txid)
       << "\",\"mempool_size\":" << e.mempool_size << "}";
    events_.push_back(os.str());
  });
  bus_.subscribe<observe::TxRemoved>([this](const observe::TxRemoved& e) {
    std::ostringstream os;
    os << "{\"type\":\"tx_removed\",\"mempool_size\":" << e.mempool_size << "}";
    events_.push_back(os.str());
  });
}

const AccountStateMachine& DemoNode::read_state() const {
  if (engine_) {
    return dynamic_cast<const AccountStateMachine&>(engine_->ledger().state());
  }
  return *genesis_;  // pre-start: read the (still-mutable) genesis state
}

void DemoNode::credit(const Hash& address, codec::u128 amount) {
  if (engine_) throw GenesisSealedError();
  genesis_->credit_genesis(address, amount);
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

std::uint64_t DemoNode::produce_block(std::uint64_t timestamp) {
  ensure_started();
  auto txs = mempool_.select(engine_->ledger().state(), 1000);
  Block blk = engine_->propose(std::move(txs), timestamp);
  auto cert = engine_->try_commit(blk, {engine_->precommit(blk, 0, validator_)});
  if (!cert) throw std::runtime_error("demo_node: failed to reach quorum");
  if (auto e = engine_->finalize(blk, *cert); !e.ok) {
    throw std::runtime_error("demo_node: finalize failed: " + e.error);
  }
  std::vector<Hash> ids;
  for (const auto& t : blk.transactions) ids.push_back(t.txid());
  mempool_.remove(ids);
  return engine_->height();
}

std::uint64_t DemoNode::height() const { return engine_ ? engine_->height() : 0; }

codec::u128 DemoNode::balance(const Hash& address) const {
  return read_state().account(address).balance;
}
std::uint64_t DemoNode::nonce(const Hash& address) const {
  return read_state().account(address).nonce;
}
Hash DemoNode::state_root() const { return read_state().state_root(); }

std::string DemoNode::drain_events_json() {
  std::string out = "[";
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (i) out.push_back(',');
    out += events_[i];
  }
  out.push_back(']');
  events_.clear();
  return out;
}

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
