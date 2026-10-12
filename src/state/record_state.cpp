#include "aviary/record_state.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/merkle.hpp"

namespace aviary {

namespace {

codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  std::copy_n(b.begin(), h.size(), h.begin());
  return h;
}

constexpr std::uint8_t kRecordLeafTag = 0x01;
constexpr std::uint8_t kNonceLeafTag = 0x02;

}  // namespace

const RecordStateMachine::Record* RecordStateMachine::record(const Hash& key) const {
  const auto it = records_.find(key);
  return it == records_.end() ? nullptr : &it->second;
}

std::uint64_t RecordStateMachine::nonce(const Hash& address) const {
  const auto it = nonces_.find(address);
  return it == nonces_.end() ? 0 : it->second;
}

RecordStateMachine::Write RecordStateMachine::decode_write(const Bytes& body) {
  codec::CodecReader r(view(body));
  Write w;
  w.nonce = r.read_u64();
  w.key = to_hash(r.read_fixed_bytes(32));
  w.op = r.read_u8();
  w.value = r.read_bytes();
  r.assert_fully_consumed();
  return w;
}

Bytes RecordStateMachine::encode_write(const Write& w) {
  codec::CodecWriter out;
  out.write_u64(w.nonce);
  out.write_fixed_bytes(view(w.key), 32);
  out.write_u8(w.op);
  out.write_bytes(view(w.value));
  return out.take();
}

std::vector<Bytes> RecordStateMachine::leaf_preimages() const {
  std::vector<Bytes> out;
  out.reserve(records_.size() + nonces_.size());
  for (const auto& [key, rec] : records_) {
    codec::CodecWriter w;
    w.write_u8(kRecordLeafTag);
    w.write_fixed_bytes(view(key), 32);
    w.write_fixed_bytes(view(rec.owner), 32);
    w.write_u64(rec.version);
    w.write_bytes(view(rec.value));
    out.push_back(w.take());
  }
  for (const auto& [addr, n] : nonces_) {
    codec::CodecWriter w;
    w.write_u8(kNonceLeafTag);
    w.write_fixed_bytes(view(addr), 32);
    w.write_u64(n);
    out.push_back(w.take());
  }
  return out;
}

Hash RecordStateMachine::state_root() const {
  std::vector<Hash> leaves;
  for (const Bytes& p : leaf_preimages()) leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, p));
  return merkle::compute_root(leaves);
}

ValidationResult RecordStateMachine::validate(const Transaction& tx) const {
  if (tx.algo() == SigAlgo::kEd25519) {
    if (tx.author.size() != 32) return ValidationResult::failure("Ed25519 author is not a 32-byte public key");
  } else if (tx.author.empty()) {
    return ValidationResult::failure("empty author");
  }
  Write w;
  try {
    w = decode_write(tx.body);
  } catch (const codec::CodecError& e) {
    return ValidationResult::failure(std::string("malformed body: ") + e.what());
  }
  const Hash author = crypto::address_of(view(tx.author));
  if (w.nonce != nonce(author)) return ValidationResult::failure("nonce mismatch");
  if (w.value.size() > kMaxValue) return ValidationResult::failure("value exceeds 64 KiB");
  const Record* existing = record(w.key);
  if (existing && existing->owner != author) return ValidationResult::failure("not the record owner");
  switch (w.op) {
    case kPut:
      return ValidationResult::success();
    case kTransfer:
      if (!existing) return ValidationResult::failure("no such record");
      if (w.value.size() != 32) return ValidationResult::failure("new owner is not a 32-byte address");
      return ValidationResult::success();
    case kDelete:
      if (!existing) return ValidationResult::failure("no such record");
      if (!w.value.empty()) return ValidationResult::failure("delete carries a value");
      return ValidationResult::success();
    default:
      return ValidationResult::failure("unknown record op");
  }
}

void RecordStateMachine::apply(const Transaction& tx) {
  const Write w = decode_write(tx.body);
  const Hash author = crypto::address_of(view(tx.author));
  nonces_[author] = nonce(author) + 1;
  switch (w.op) {
    case kPut: {
      Record& rec = records_[w.key];
      if (rec.version == 0) rec.owner = author;
      rec.version += 1;
      rec.value = w.value;
      return;
    }
    case kTransfer: {
      Record& rec = records_[w.key];
      rec.owner = to_hash(w.value);
      rec.version += 1;
      return;
    }
    default:
      records_.erase(w.key);
  }
}

std::unique_ptr<StateMachine> RecordStateMachine::clone() const {
  return std::make_unique<RecordStateMachine>(*this);
}

Bytes RecordStateMachine::serialize() const {
  codec::CodecWriter w;
  w.write_u32(static_cast<std::uint32_t>(records_.size()));
  for (const auto& [key, rec] : records_) {
    w.write_fixed_bytes(view(key), 32);
    w.write_fixed_bytes(view(rec.owner), 32);
    w.write_u64(rec.version);
    w.write_bytes(view(rec.value));
  }
  w.write_u32(static_cast<std::uint32_t>(nonces_.size()));
  for (const auto& [addr, n] : nonces_) {
    w.write_fixed_bytes(view(addr), 32);
    w.write_u64(n);
  }
  return w.take();
}

std::unique_ptr<RecordStateMachine> RecordStateMachine::deserialize(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  auto sm = std::make_unique<RecordStateMachine>();
  const std::uint32_t n = r.read_u32();
  for (std::uint32_t i = 0; i < n; ++i) {
    const Hash key = to_hash(r.read_fixed_bytes(32));
    Record rec;
    rec.owner = to_hash(r.read_fixed_bytes(32));
    rec.version = r.read_u64();
    rec.value = r.read_bytes();
    sm->records_[key] = std::move(rec);
  }
  const std::uint32_t m = r.read_u32();
  for (std::uint32_t i = 0; i < m; ++i) {
    const Hash addr = to_hash(r.read_fixed_bytes(32));
    sm->nonces_[addr] = r.read_u64();
  }
  r.assert_fully_consumed();
  return sm;
}

StateProof RecordStateMachine::prove_record(const Hash& key) const {
  const std::vector<Bytes> preimages = leaf_preimages();
  std::vector<Hash> leaves;
  leaves.reserve(preimages.size());
  StateProof proof;
  std::uint32_t index = 0;
  bool found = false;
  for (std::uint32_t i = 0; i < preimages.size(); ++i) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, preimages[i]));
    if (i < records_.size() && !found) {
      auto it = records_.begin();
      std::advance(it, i);
      if (it->first == key) {
        found = true;
        index = i;
        proof.leaf_preimage = preimages[i];
      }
    }
  }
  proof.leaf_count = static_cast<std::uint32_t>(leaves.size());
  proof.present = found;
  if (found) proof.proof = merkle::make_proof(leaves, index);
  return proof;
}

RecordStateMachine::RecordLeaf RecordStateMachine::decode_record_leaf(const Bytes& preimage) {
  codec::CodecReader r(view(preimage));
  if (r.read_u8() != kRecordLeafTag) throw codec::CodecError("not a record leaf");
  RecordLeaf leaf;
  leaf.key = to_hash(r.read_fixed_bytes(32));
  leaf.owner = to_hash(r.read_fixed_bytes(32));
  leaf.version = r.read_u64();
  leaf.value = r.read_bytes();
  r.assert_fully_consumed();
  return leaf;
}

}  // namespace aviary
