#include <map>
#include <memory>
#include "aviary/utxo_state.hpp"

#include <set>
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

Bytes UtxoStateMachine::encode_witnesses(const std::vector<Witness>& ws) {
  codec::CodecWriter w;
  w.write_list(ws, [](codec::CodecWriter& w, const Witness& x) {
    w.write_bytes(view(x.pubkey));
    w.write_bytes(view(x.signature));
  });
  return w.take();
}

std::vector<UtxoStateMachine::Witness> UtxoStateMachine::decode_witnesses(const Bytes& bytes) {
  codec::CodecReader r(view(bytes));
  auto ws = r.read_list<Witness>([](codec::CodecReader& r) {
    Witness x;
    x.pubkey = r.read_bytes();
    x.signature = r.read_bytes();
    return x;
  });
  r.assert_fully_consumed();
  return ws;
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

UtxoStateMachine::UtxoStateMachine(codec::u128 flat_fee)
    : base_(std::make_shared<Snapshot>()), flat_fee_(flat_fee) {}

const UtxoStateMachine::Output* UtxoStateMachine::lookup(const Outpoint& op) const {
  if (auto it = overlay_.find(op); it != overlay_.end()) {
    return it->second ? &*it->second : nullptr;  // nullopt = spent (tombstone)
  }
  if (auto it = base_->utxos.find(op); it != base_->utxos.end()) return &it->second;
  return nullptr;
}

std::map<UtxoStateMachine::Outpoint, UtxoStateMachine::Output> UtxoStateMachine::merged() const {
  std::map<Outpoint, Output> out = base_->utxos;
  for (const auto& [op, v] : overlay_) {
    if (v) {
      out[op] = *v;
    } else {
      out.erase(op);
    }
  }
  return out;
}

void UtxoStateMachine::seal() const {
  if (overlay_.empty()) return;
  auto snap = std::make_shared<Snapshot>();
  snap->utxos = base_->utxos;
  for (const auto& [op, v] : overlay_) {
    if (v) {
      snap->utxos[op] = *v;
    } else {
      snap->utxos.erase(op);
    }
  }
  snap->sorted_ops.reserve(snap->utxos.size());
  std::vector<Hash> leaves;
  leaves.reserve(snap->utxos.size());
  for (const auto& [op, out] : snap->utxos) {  // map iterates outpoint-sorted
    snap->sorted_ops.push_back(op);
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, utxo_leaf_preimage(op, out)));
  }
  snap->tree.reset(std::move(leaves));
  base_ = std::move(snap);
  overlay_.clear();
}

UtxoStateMachine::Outpoint UtxoStateMachine::add_genesis_utxo(const Output& output) {
  Outpoint op;
  op.txid = crypto::zero_hash();
  op.index = static_cast<std::uint32_t>(genesis_counter_++);
  overlay_[op] = output;
  return op;
}

UtxoStateMachine::Output UtxoStateMachine::get(const Outpoint& op) const {
  const Output* o = lookup(op);
  return o ? *o : Output{};
}

std::size_t UtxoStateMachine::utxo_count() const {
  // base minus the overlay's tombstones plus its fresh outpoints.
  std::size_t n = base_->utxos.size();
  for (const auto& [op, v] : overlay_) {
    const bool in_base = base_->utxos.count(op) != 0;
    if (v && !in_base) ++n;       // newly created
    else if (!v && in_base) --n;  // spent a committed one
  }
  return n;
}

Hash UtxoStateMachine::state_root() const {
  // UTXO blocks add/remove outpoints (structural), so the incremental-update
  // path never applies - materialise once when there are pending changes.
  if (overlay_.empty()) return base_->tree.root();
  seal();
  return base_->tree.root();
}

StateProof UtxoStateMachine::prove_outpoint(const Outpoint& target) const {
  const std::map<Outpoint, Output> utxos = merged();
  std::vector<Hash> leaves;
  leaves.reserve(utxos.size());
  StateProof proof;
  bool found = false;
  std::uint32_t found_index = 0;
  std::uint32_t i = 0;
  for (const auto& [op, out] : utxos) {
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
  // Authorization mode: single-author (one owner for all inputs) or multi-signer
  // (a per-input witness). verify_transaction_signature already checked the
  // single-author envelope signature; witness signatures are checked here,
  // where each input's owner is known.
  const bool witness_mode = tx.algo() == SigAlgo::kUtxoWitness;
  if (!witness_mode && tx.author.size() != 32) {
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

  std::vector<Witness> witnesses;
  Bytes signing;
  Hash author_addr{};
  if (witness_mode) {
    try {
      witnesses = decode_witnesses(tx.signature);
    } catch (const codec::CodecError& e) {
      return ValidationResult::failure(std::string("malformed witnesses: ") + e.what());
    }
    if (witnesses.size() != b.inputs.size()) {
      return ValidationResult::failure("witness count does not match input count");
    }
    signing = tx.signing_bytes();
  } else {
    author_addr = crypto::address_of(view(tx.author));
  }

  // Inputs must be distinct, exist, and be authorized by their owner.
  std::set<Outpoint> seen;
  codec::u128 in_sum = 0;
  for (std::size_t i = 0; i < b.inputs.size(); ++i) {
    const Outpoint& op = b.inputs[i];
    if (!seen.insert(op).second) {
      return ValidationResult::failure("duplicate input (double-spend within tx)");
    }
    const Output* utxo = lookup(op);
    if (utxo == nullptr) {
      return ValidationResult::failure("input not found / already spent");
    }
    if (witness_mode) {
      const Witness& wt = witnesses[i];
      if (wt.pubkey.size() != 32) {
        return ValidationResult::failure("witness public key is not 32 bytes");
      }
      if (crypto::address_of(view(wt.pubkey)) != utxo->owner) {
        return ValidationResult::failure("witness does not own its input");
      }
      if (wt.signature.size() != 64 ||
          !crypto::ed25519_verify(view(signing), view(wt.signature), view(wt.pubkey))) {
        return ValidationResult::failure("invalid witness signature");
      }
    } else if (utxo->owner != author_addr) {
      return ValidationResult::failure("input not owned by signer");
    }
    if (in_sum > kU128Max - utxo->amount) {
      return ValidationResult::failure("input sum overflow");
    }
    in_sum += utxo->amount;
  }

  codec::u128 out_sum = 0;
  for (const auto& o : b.outputs) {
    if (out_sum > kU128Max - o.amount) {
      return ValidationResult::failure("output sum overflow");
    }
    out_sum += o.amount;
  }

  // Value must be conserved OR burned as a fee: outputs may not exceed inputs,
  // and the surplus (the fee) must clear the chain minimum. flat_fee_ == 0 keeps
  // the strict conservation rule (in == out).
  if (out_sum > in_sum) {
    return ValidationResult::failure("value not conserved (outputs exceed inputs)");
  }
  if (in_sum - out_sum < flat_fee_) {
    return ValidationResult::failure("fee below chain minimum");
  }
  return ValidationResult::success();
}

void UtxoStateMachine::apply(const Transaction& tx) {
  // Precondition: validate(tx).ok. The fee (inputs - outputs) is burned simply
  // by not creating an output for it.
  const Body b = decode_body(tx.body);
  for (const auto& op : b.inputs) overlay_[op] = std::nullopt;  // tombstone: spent

  const Hash txid = tx.txid();
  for (std::uint32_t i = 0; i < b.outputs.size(); ++i) {
    overlay_[Outpoint{txid, i}] = b.outputs[i];
  }
}

codec::u128 UtxoStateMachine::tx_fee(const Transaction& tx) const {
  // The implicit fee is sum(inputs) - sum(outputs) against the current UTXO set.
  Body b;
  try {
    b = decode_body(tx.body);
  } catch (const codec::CodecError&) {
    return 0;
  }
  codec::u128 in_sum = 0, out_sum = 0;
  for (const auto& op : b.inputs) {
    if (const Output* u = lookup(op)) in_sum += u->amount;
  }
  for (const auto& o : b.outputs) out_sum += o.amount;
  return in_sum >= out_sum ? in_sum - out_sum : 0;
}

std::unique_ptr<StateMachine> UtxoStateMachine::clone() const {
  auto c = std::make_unique<UtxoStateMachine>(flat_fee_);
  c->base_ = base_;        // share the immutable committed snapshot (O(1))
  c->overlay_ = overlay_;  // copy only the uncommitted changes
  c->genesis_counter_ = genesis_counter_;
  return c;
}

Bytes UtxoStateMachine::serialize() const {
  seal();  // materialise the overlay so base_ holds the full committed UTXO set
  codec::CodecWriter w;
  w.write_u128(flat_fee_);
  w.write_u64(genesis_counter_);
  w.write_u32(static_cast<std::uint32_t>(base_->utxos.size()));
  for (const auto& [op, out] : base_->utxos) {  // map -> outpoint-sorted, canonical
    encode_outpoint(w, op);
    encode_output(w, out);
  }
  return w.take();
}

std::unique_ptr<UtxoStateMachine> UtxoStateMachine::deserialize(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  const codec::u128 flat_fee = r.read_u128();
  auto sm = std::make_unique<UtxoStateMachine>(flat_fee);
  sm->genesis_counter_ = r.read_u64();
  const std::uint32_t n = r.read_u32();
  for (std::uint32_t i = 0; i < n; ++i) {
    Outpoint op = decode_outpoint(r);
    sm->overlay_[op] = decode_output(r);
  }
  r.assert_fully_consumed();
  sm->seal();
  return sm;
}

}  // namespace aviary
