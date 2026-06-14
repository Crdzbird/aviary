#include "blockchainer/account_state.hpp"

#include <vector>

#include "blockchainer/merkle.hpp"

namespace blockchainer {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

constexpr codec::u128 kU128Max = ~static_cast<codec::u128>(0);

// Canonical leaf preimage for an account: addr(32) ‖ balance(u128) ‖ nonce(u64).
Bytes account_leaf_preimage(const Hash& addr, const AccountStateMachine::Account& acc) {
  codec::CodecWriter w;
  w.write_fixed_bytes(view(addr), 32);
  w.write_u128(acc.balance);
  w.write_u64(acc.nonce);
  return w.take();
}
}  // namespace

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

void AccountStateMachine::credit_genesis(const Hash& address, codec::u128 amount) {
  accounts_[address].balance += amount;
}

AccountStateMachine::Account AccountStateMachine::account(const Hash& address) const {
  auto it = accounts_.find(address);
  return it == accounts_.end() ? Account{} : it->second;
}

Hash AccountStateMachine::state_root() const {
  std::vector<Hash> leaves;
  leaves.reserve(accounts_.size());
  for (const auto& [addr, acc] : accounts_) {  // map iterates sorted by address
    leaves.push_back(
        crypto::hash_tagged(crypto::Domain::kLeaf, account_leaf_preimage(addr, acc)));
  }
  return merkle::compute_root(leaves);
}

StateProof AccountStateMachine::prove_account(const Hash& address) const {
  std::vector<Hash> leaves;
  leaves.reserve(accounts_.size());
  StateProof proof;
  bool found = false;
  std::uint32_t found_index = 0;
  std::uint32_t i = 0;
  for (const auto& [addr, acc] : accounts_) {
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
  if (tx.author.size() != 32) {
    return ValidationResult::failure("author is not a 32-byte public key");
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
  if (acc.balance < t.amount) {
    return ValidationResult::failure("insufficient balance");
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

  accounts_[sender].balance -= t.amount;
  accounts_[sender].nonce += 1;
  accounts_[t.recipient].balance += t.amount;  // self-transfer nets to zero
}

std::unique_ptr<StateMachine> AccountStateMachine::clone() const {
  return std::make_unique<AccountStateMachine>(*this);
}

}  // namespace blockchainer
