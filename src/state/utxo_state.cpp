#include "blockchainer/utxo_state.hpp"

#include <set>
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

void encode_outpoint(codec::CodecWriter& w, const UtxoStateMachine::Outpoint& op) {
  w.write_fixed_bytes(view(op.txid), 32);
  w.write_u32(op.index);
}
void encode_output(codec::CodecWriter& w, const UtxoStateMachine::Output& o) {
  w.write_fixed_bytes(view(o.owner), 32);
  w.write_u128(o.amount);
}
UtxoStateMachine::Outpoint decode_outpoint(codec::CodecReader& r) {
  UtxoStateMachine::Outpoint op;
  op.txid = to_hash(r.read_fixed_bytes(32));
  op.index = r.read_u32();
  return op;
}
UtxoStateMachine::Output decode_output(codec::CodecReader& r) {
  UtxoStateMachine::Output o;
  o.owner = to_hash(r.read_fixed_bytes(32));
  o.amount = r.read_u128();
  return o;
}
}  // namespace

UtxoStateMachine::Body UtxoStateMachine::decode_body(const Bytes& body) {
  codec::CodecReader r(view(body));
  Body b;
  b.inputs = r.read_list<Outpoint>([](codec::CodecReader& r) { return decode_outpoint(r); });
  b.outputs = r.read_list<Output>([](codec::CodecReader& r) { return decode_output(r); });
  r.assert_fully_consumed();
  return b;
}

Bytes UtxoStateMachine::encode_body(const Body& b) {
  codec::CodecWriter w;
  w.write_list(b.inputs, [](codec::CodecWriter& w, const Outpoint& op) { encode_outpoint(w, op); });
  w.write_list(b.outputs, [](codec::CodecWriter& w, const Output& o) { encode_output(w, o); });
  return w.take();
}

UtxoStateMachine::Outpoint UtxoStateMachine::add_genesis_utxo(const Output& output) {
  Outpoint op;
  op.txid = crypto::zero_hash();
  op.index = static_cast<std::uint32_t>(genesis_counter_++);
  utxos_[op] = output;
  return op;
}

UtxoStateMachine::Output UtxoStateMachine::get(const Outpoint& op) const {
  auto it = utxos_.find(op);
  return it == utxos_.end() ? Output{} : it->second;
}

namespace {
Bytes utxo_leaf_preimage(const UtxoStateMachine::Outpoint& op,
                         const UtxoStateMachine::Output& out) {
  codec::CodecWriter w;
  encode_outpoint(w, op);
  encode_output(w, out);
  return w.take();
}
}  // namespace

Hash UtxoStateMachine::state_root() const {
  std::vector<Hash> leaves;
  leaves.reserve(utxos_.size());
  for (const auto& [op, out] : utxos_) {  // map iterates sorted by outpoint
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, utxo_leaf_preimage(op, out)));
  }
  return merkle::compute_root(leaves);
}

StateProof UtxoStateMachine::prove_outpoint(const Outpoint& target) const {
  std::vector<Hash> leaves;
  leaves.reserve(utxos_.size());
  StateProof proof;
  bool found = false;
  std::uint32_t found_index = 0;
  std::uint32_t i = 0;
  for (const auto& [op, out] : utxos_) {
    Bytes preimage = utxo_leaf_preimage(op, out);
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, preimage));
    if (op == target) {
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

ValidationResult UtxoStateMachine::validate(const Transaction& tx) const {
  if (tx.author.size() != 32) {
    return ValidationResult::failure("author is not a 32-byte public key");
  }
  Body b;
  try {
    b = decode_body(tx.body);
  } catch (const codec::CodecError& e) {
    return ValidationResult::failure(std::string("malformed body: ") + e.what());
  }
  if (b.inputs.empty()) {
    return ValidationResult::failure("no inputs (minting is not allowed)");
  }

  const Hash author = crypto::address_of(view(tx.author));

  // Inputs must be distinct, exist, and be owned by the author.
  std::set<Outpoint> seen;
  codec::u128 in_sum = 0;
  for (const auto& op : b.inputs) {
    if (!seen.insert(op).second) {
      return ValidationResult::failure("duplicate input (double-spend within tx)");
    }
    auto it = utxos_.find(op);
    if (it == utxos_.end()) {
      return ValidationResult::failure("input not found / already spent");
    }
    if (it->second.owner != author) {
      return ValidationResult::failure("input not owned by signer");
    }
    if (in_sum > kU128Max - it->second.amount) {
      return ValidationResult::failure("input sum overflow");
    }
    in_sum += it->second.amount;
  }

  codec::u128 out_sum = 0;
  for (const auto& o : b.outputs) {
    if (out_sum > kU128Max - o.amount) {
      return ValidationResult::failure("output sum overflow");
    }
    out_sum += o.amount;
  }

  if (in_sum != out_sum) {
    return ValidationResult::failure("value not conserved (inputs != outputs)");
  }
  return ValidationResult::success();
}

void UtxoStateMachine::apply(const Transaction& tx) {
  // Precondition: validate(tx).ok.
  const Body b = decode_body(tx.body);
  for (const auto& op : b.inputs) utxos_.erase(op);

  const Hash txid = tx.txid();
  for (std::uint32_t i = 0; i < b.outputs.size(); ++i) {
    utxos_[Outpoint{txid, i}] = b.outputs[i];
  }
}

std::unique_ptr<StateMachine> UtxoStateMachine::clone() const {
  return std::make_unique<UtxoStateMachine>(*this);
}

}  // namespace blockchainer
