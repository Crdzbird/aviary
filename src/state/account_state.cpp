#include <map>
#include <memory>
#include "aviary/account_state.hpp"

#include <algorithm>
#include <vector>

#include "aviary/merkle.hpp"

namespace aviary {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

constexpr codec::u128 kU128Max = ~static_cast<codec::u128>(0);

// Seal the overlay into a fresh baseline when it grows past this fraction of the
// committed state (1/kSealDivisor), amortising the O(N) rebuild over many
// blocks. Below the threshold, state_root() stays on the O(K·log N) incremental
// path. Also a small absolute floor so tiny states just rebuild.
constexpr std::size_t kSealDivisor = 8;
constexpr std::size_t kSealFloor = 64;

// Canonical leaf preimage for an account: addr(32) ‖ balance(u128) ‖ nonce(u64).
Bytes account_leaf_preimage(const Hash& addr, const AccountStateMachine::Account& acc) {
  codec::CodecWriter w;
  w.write_fixed_bytes(view(addr), 32);
  w.write_u128(acc.balance);
  w.write_u64(acc.nonce);
  return w.take();
}

Hash leaf_hash(const Hash& addr, const AccountStateMachine::Account& acc) {
  return crypto::hash_tagged(crypto::Domain::kLeaf, account_leaf_preimage(addr, acc));
}
}  // namespace

AccountStateMachine::AccountStateMachine(codec::u128 flat_fee)
    : base_(std::make_shared<Snapshot>()), flat_fee_(flat_fee) {}

AccountStateMachine::Transfer AccountStateMachine::decode_transfer(const Bytes& body) {
  codec::CodecReader r(view(body));
  Transfer t;
  t.nonce = r.read_u64();
  t.recipient = to_hash(r.read_fixed_bytes(32));
  t.amount = r.read_u128();
  r.assert_fully_consumed();
  return t;
}

Bytes AccountStateMachine::encode_transfer(const Transfer& t) {
  codec::CodecWriter w;
  w.write_u64(t.nonce);
  w.write_fixed_bytes(view(t.recipient), 32);
  w.write_u128(t.amount);
  return w.take();
}

const AccountStateMachine::Account* AccountStateMachine::lookup(const Hash& address) const {
  if (auto it = overlay_.find(address); it != overlay_.end()) return &it->second;
  if (auto it = base_->accounts.find(address); it != base_->accounts.end()) return &it->second;
  return nullptr;
}

void AccountStateMachine::put(const Hash& address, const Account& acc) {
  overlay_[address] = acc;
}

std::map<Hash, AccountStateMachine::Account> AccountStateMachine::merged() const {
  std::map<Hash, Account> out = base_->accounts;
  for (const auto& [addr, acc] : overlay_) out[addr] = acc;
  return out;
}

void AccountStateMachine::seal() const {
  if (overlay_.empty()) return;
  auto snap = std::make_shared<Snapshot>();
  snap->accounts = base_->accounts;
  for (const auto& [addr, acc] : overlay_) snap->accounts[addr] = acc;

  snap->sorted_addrs.reserve(snap->accounts.size());
  std::vector<Hash> leaves;
  leaves.reserve(snap->accounts.size());
  for (const auto& [addr, acc] : snap->accounts) {  // map iterates address-sorted
    snap->sorted_addrs.push_back(addr);
    leaves.push_back(leaf_hash(addr, acc));
  }
  snap->tree.reset(std::move(leaves));

  base_ = std::move(snap);
  overlay_.clear();
}

void AccountStateMachine::credit_genesis(const Hash& address, codec::u128 amount) {
  Account acc = account(address);
  acc.balance += amount;
  put(address, acc);
}

AccountStateMachine::Account AccountStateMachine::account(const Hash& address) const {
  const Account* a = lookup(address);
  return a ? *a : Account{};
}

Hash AccountStateMachine::state_root() const {
  if (overlay_.empty()) return base_->tree.root();

  // Any brand-new account shifts leaf positions, so the incremental path (which
  // only rewrites existing positions) does not apply — rebuild. Likewise once
  // the overlay is a big fraction of the state, rebuilding amortises better than
  // repeatedly walking a large change set.
  bool has_inserts = false;
  for (const auto& [addr, acc] : overlay_) {
    if (!base_->accounts.count(addr)) {
      has_inserts = true;
      break;
    }
  }
  const std::size_t base_leaves = base_->sorted_addrs.size();
  const std::size_t seal_threshold = std::max(kSealFloor, base_leaves / kSealDivisor);
  if (has_inserts || overlay_.size() > seal_threshold) {
    seal();
    return base_->tree.root();
  }

  // Updates-only, small overlay: recompute just the changed leaves' root-paths
  // against the cached committed tree — O(K·log N), no rebuild, no mutation.
  std::map<std::size_t, Hash> changes;
  for (const auto& [addr, acc] : overlay_) {
    auto it = std::lower_bound(base_->sorted_addrs.begin(), base_->sorted_addrs.end(), addr);
    const std::size_t idx = static_cast<std::size_t>(it - base_->sorted_addrs.begin());
    changes[idx] = leaf_hash(addr, acc);
  }
  return base_->tree.root_with_updates(changes);
}

StateProof AccountStateMachine::prove_account(const Hash& address) const {
  // Proofs are off the hot path; materialise so we prove against a coherent
  // committed tree.
  const std::map<Hash, Account> accounts = merged();
  std::vector<Hash> leaves;
  leaves.reserve(accounts.size());
  StateProof proof;
  bool found = false;
  std::uint32_t found_index = 0;
  std::uint32_t i = 0;
  for (const auto& [addr, acc] : accounts) {
    Bytes preimage = account_leaf_preimage(addr, acc);
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, preimage));
    if (addr == address) {
      found = true;
      found_index = i;
      proof.leaf_preimage = std::move(preimage);
    }
    ++i;
  }
  proof.leaf_count = static_cast<std::uint32_t>(leaves.size());
  proof.present = found;
  if (found) proof.proof = merkle::make_proof(leaves, found_index);
  return proof;
}

AccountStateMachine::AccountLeaf AccountStateMachine::decode_account_leaf(
    const Bytes& preimage) {
  codec::CodecReader r(view(preimage));
  AccountLeaf leaf;
  leaf.address = to_hash(r.read_fixed_bytes(32));
  leaf.balance = r.read_u128();
  leaf.nonce = r.read_u64();
  r.assert_fully_consumed();
  return leaf;
}

ValidationResult AccountStateMachine::validate(const Transaction& tx) const {
  // The sender address is BLAKE3(author) for any scheme. Ed25519 keys are a
  // fixed 32 bytes; hybrid (PQ) public keys are larger, and their exact shape is
  // validated by signature verification (run before validate/apply in the ledger
  // and mempool). So enforce the fixed size only for Ed25519.
  if (tx.algo() == SigAlgo::kEd25519) {
    if (tx.author.size() != 32) {
      return ValidationResult::failure("Ed25519 author is not a 32-byte public key");
    }
  } else if (tx.author.empty()) {
    return ValidationResult::failure("empty author");
  }
  Transfer t;
  try {
    t = decode_transfer(tx.body);
  } catch (const codec::CodecError& e) {
    return ValidationResult::failure(std::string("malformed body: ") + e.what());
  }

  const Hash sender = crypto::address_of(view(tx.author));
  const Account acc = account(sender);

  if (t.nonce != acc.nonce) {
    return ValidationResult::failure("nonce mismatch");
  }
  // Sender must cover the transfer plus the total fee: the flat protocol base
  // fee plus the sender's declared priority tip. Guard every sum against u128
  // overflow (an overflow would be a fork).
  const codec::u128 tip = tx.declared_fee();
  if (flat_fee_ > kU128Max - tip) {
    return ValidationResult::failure("fee overflow");
  }
  const codec::u128 total_fee = flat_fee_ + tip;
  if (total_fee > kU128Max - t.amount) {
    return ValidationResult::failure("amount + fee overflow");
  }
  if (acc.balance < t.amount + total_fee) {
    return ValidationResult::failure("insufficient balance for amount + fee");
  }
  // Guard recipient credit against u128 overflow (overflow would be a fork).
  if (sender != t.recipient) {
    const Account rec = account(t.recipient);
    if (rec.balance > kU128Max - t.amount) {
      return ValidationResult::failure("recipient balance overflow");
    }
  }
  return ValidationResult::success();
}

void AccountStateMachine::apply(const Transaction& tx) {
  // Precondition: validate(tx).ok — fields here are known good.
  const Transfer t = decode_transfer(tx.body);
  const Hash sender = crypto::address_of(view(tx.author));

  Account s = account(sender);
  s.balance -= t.amount + flat_fee_ + tx.declared_fee();  // fees burned, not credited
  s.nonce += 1;
  put(sender, s);
  // Read the recipient AFTER writing the sender so a self-transfer nets to −fee
  // (overlay reflects the just-debited balance).
  Account r = account(t.recipient);
  r.balance += t.amount;
  put(t.recipient, r);
}

codec::u128 AccountStateMachine::tx_fee(const Transaction& tx) const {
  // The fee is state-independent for the account model: the flat base fee plus
  // the sender's declared priority tip.
  return flat_fee_ + tx.declared_fee();
}

std::unique_ptr<StateMachine> AccountStateMachine::clone() const {
  auto c = std::make_unique<AccountStateMachine>(flat_fee_);
  c->base_ = base_;        // share the immutable committed snapshot (O(1))
  c->overlay_ = overlay_;  // copy only the uncommitted changes
  return c;
}

Bytes AccountStateMachine::serialize() const {
  seal();  // materialise the overlay so base_ holds the full committed state
  codec::CodecWriter w;
  w.write_u128(flat_fee_);
  w.write_u32(static_cast<std::uint32_t>(base_->accounts.size()));
  for (const auto& [addr, acc] : base_->accounts) {  // map → address-sorted, canonical
    w.write_fixed_bytes(view(addr), 32);
    w.write_u128(acc.balance);
    w.write_u64(acc.nonce);
  }
  return w.take();
}

std::unique_ptr<AccountStateMachine> AccountStateMachine::deserialize(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  const codec::u128 flat_fee = r.read_u128();
  auto sm = std::make_unique<AccountStateMachine>(flat_fee);
  const std::uint32_t n = r.read_u32();
  for (std::uint32_t i = 0; i < n; ++i) {
    Hash addr = to_hash(r.read_fixed_bytes(32));
    Account acc;
    acc.balance = r.read_u128();
    acc.nonce = r.read_u64();
    sm->overlay_[addr] = acc;
  }
  r.assert_fully_consumed();
  sm->seal();  // commit into the base snapshot
  return sm;
}

}  // namespace aviary
