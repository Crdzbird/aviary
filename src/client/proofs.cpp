#include "aviary/proofs.hpp"

namespace aviary {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}

void encode_merkle_proof(codec::CodecWriter& w, const merkle::Proof& p) {
  w.write_u32(p.index);
  w.write_list(p.siblings, [](codec::CodecWriter& w, const Hash& s) {
    w.write_fixed_bytes(view(s), 32);
  });
}

merkle::Proof decode_merkle_proof(codec::CodecReader& r) {
  merkle::Proof p;
  p.index = r.read_u32();
  p.siblings = r.read_list<Hash>([](codec::CodecReader& r) { return to_hash(r.read_fixed_bytes(32)); });
  return p;
}
}  // namespace

// ── StateProof ───────────────────────────────────────────────────────────────

Bytes StateProof::encode() const {
  codec::CodecWriter w;
  w.write_bool(present);
  w.write_bytes(view(leaf_preimage));
  w.write_u32(leaf_count);
  encode_merkle_proof(w, proof);
  return w.take();
}

StateProof StateProof::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  StateProof p;
  p.present = r.read_bool();
  p.leaf_preimage = r.read_bytes();
  p.leaf_count = r.read_u32();
  p.proof = decode_merkle_proof(r);
  r.assert_fully_consumed();
  return p;
}

bool verify_state_proof(const Hash& state_root, const StateProof& p) {
  if (!p.present) return false;  // absence proofs not supported (see header)
  const Hash leaf = crypto::hash_tagged(crypto::Domain::kLeaf, p.leaf_preimage);
  return merkle::verify_proof(state_root, leaf, p.proof, p.leaf_count);
}

// ── TxInclusionProof ─────────────────────────────────────────────────────────

Bytes TxInclusionProof::encode() const {
  codec::CodecWriter w;
  w.write_bytes(view(tx_encoding));
  w.write_u32(leaf_count);
  encode_merkle_proof(w, proof);
  return w.take();
}

TxInclusionProof TxInclusionProof::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  TxInclusionProof p;
  p.tx_encoding = r.read_bytes();
  p.leaf_count = r.read_u32();
  p.proof = decode_merkle_proof(r);
  r.assert_fully_consumed();
  return p;
}

TxInclusionProof tx_inclusion_proof(const Block& block, std::uint32_t index) {
  std::vector<Hash> leaves;
  leaves.reserve(block.transactions.size());
  for (const auto& tx : block.transactions) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, tx.encode()));
  }
  TxInclusionProof p;
  p.tx_encoding = block.transactions.at(index).encode();
  p.leaf_count = static_cast<std::uint32_t>(leaves.size());
  p.proof = merkle::make_proof(leaves, index);
  return p;
}

bool verify_tx_inclusion(const Hash& tx_root, const TxInclusionProof& p) {
  const Hash leaf = crypto::hash_tagged(crypto::Domain::kLeaf, p.tx_encoding);
  return merkle::verify_proof(tx_root, leaf, p.proof, p.leaf_count);
}

// ── Receipt inclusion ──────────────────────────────────────────────────────────

Bytes ReceiptProof::encode() const {
  codec::CodecWriter w;
  w.write_bytes(view(receipt_encoding));
  w.write_u32(leaf_count);
  encode_merkle_proof(w, proof);
  return w.take();
}

ReceiptProof ReceiptProof::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  ReceiptProof p;
  p.receipt_encoding = r.read_bytes();
  p.leaf_count = r.read_u32();
  p.proof = decode_merkle_proof(r);
  r.assert_fully_consumed();
  return p;
}

ReceiptProof receipt_inclusion_proof(const std::vector<Receipt>& receipts,
                                     std::uint32_t index) {
  std::vector<Hash> leaves;
  leaves.reserve(receipts.size());
  for (const auto& rc : receipts) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, rc.encode()));
  }
  ReceiptProof p;
  p.receipt_encoding = receipts.at(index).encode();
  p.leaf_count = static_cast<std::uint32_t>(leaves.size());
  p.proof = merkle::make_proof(leaves, index);
  return p;
}

bool verify_receipt_inclusion(const Hash& receipt_root, const ReceiptProof& p) {
  const Hash leaf = crypto::hash_tagged(crypto::Domain::kLeaf, p.receipt_encoding);
  return merkle::verify_proof(receipt_root, leaf, p.proof, p.leaf_count);
}

}  // namespace aviary
