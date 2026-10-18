#include "aviary/types.hpp"

#include "aviary/merkle.hpp"

#include <stdexcept>

namespace aviary {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView view(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  // Reader guarantees b.size() == 32 via read_fixed_bytes(32).
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}
}  // namespace

// -- Transaction --------------------------------------------------------------

Bytes Transaction::signing_bytes() const {
  codec::CodecWriter w;
  w.write_u8(version);
  w.write_u64(chain_id);
  if (version >= 2) {
    w.write_u8(sig_algo);   // commit to the scheme (anti-downgrade)
    w.write_u128(fee);      // commit to the declared fee (anti-tamper)
  }
  w.write_bytes(view(body));
  w.write_bytes(view(author));
  return w.take();
}

Bytes Transaction::encode() const {
  codec::CodecWriter w;
  w.write_u8(version);
  w.write_u64(chain_id);
  if (version >= 2) {
    w.write_u8(sig_algo);
    w.write_u128(fee);
  }
  w.write_bytes(view(body));
  w.write_bytes(view(author));
  w.write_bytes(view(signature));
  return w.take();
}

Transaction Transaction::decode(codec::CodecReader& r) {
  Transaction tx;
  tx.version = r.read_u8();
  tx.chain_id = r.read_u64();
  if (tx.version >= 2) {
    tx.sig_algo = r.read_u8();
    tx.fee = r.read_u128();
  }
  tx.body = r.read_bytes();
  tx.author = r.read_bytes();
  tx.signature = r.read_bytes();
  return tx;
}

Transaction Transaction::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Transaction tx = decode(r);
  r.assert_fully_consumed();
  return tx;
}

Hash Transaction::txid() const {
  return crypto::hash_tagged(crypto::Domain::kTxId, encode());
}

// -- BlockHeader --------------------------------------------------------------

Bytes BlockHeader::encode() const {
  codec::CodecWriter w;
  w.write_u8(version);
  w.write_u64(chain_id);
  w.write_u64(height);
  w.write_fixed_bytes(view(parent_hash), 32);
  w.write_u64(timestamp);
  w.write_fixed_bytes(view(tx_root), 32);
  w.write_fixed_bytes(view(state_root), 32);
  if (version >= 2) w.write_fixed_bytes(view(receipt_root), 32);  // execution commitment
  if (version >= 3) {
    w.write_fixed_bytes(view(anchor_root), 32);    // this block's subchain anchors
    w.write_fixed_bytes(view(subchain_root), 32);  // post-block child-tip registry
  }
  w.write_bytes(view(consensus_data));
  return w.take();
}

BlockHeader BlockHeader::decode(codec::CodecReader& r) {
  BlockHeader h;
  h.version = r.read_u8();
  h.chain_id = r.read_u64();
  h.height = r.read_u64();
  h.parent_hash = to_hash(r.read_fixed_bytes(32));
  h.timestamp = r.read_u64();
  h.tx_root = to_hash(r.read_fixed_bytes(32));
  h.state_root = to_hash(r.read_fixed_bytes(32));
  if (h.version >= 2) h.receipt_root = to_hash(r.read_fixed_bytes(32));
  if (h.version >= 3) {
    h.anchor_root = to_hash(r.read_fixed_bytes(32));
    h.subchain_root = to_hash(r.read_fixed_bytes(32));
  }
  h.consensus_data = r.read_bytes();
  return h;
}

// -- CertifiedHeader / Anchor -------------------------------------------------

Bytes CertifiedHeader::encode() const {
  codec::CodecWriter w;
  Bytes hdr = header.encode();
  w.write_bytes(view(hdr));
  w.write_bytes(view(certificate));
  return w.take();
}

CertifiedHeader CertifiedHeader::decode(codec::CodecReader& r) {
  CertifiedHeader c;
  Bytes hdr = r.read_bytes();
  c.header = BlockHeader::decode_all(view(hdr));
  c.certificate = r.read_bytes();
  return c;
}

CertifiedHeader CertifiedHeader::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  CertifiedHeader c = decode(r);
  r.assert_fully_consumed();
  return c;
}

Bytes Anchor::encode() const {
  codec::CodecWriter w;
  w.write_u64(child_chain_id);
  w.write_list(headers, [](codec::CodecWriter& w, const CertifiedHeader& c) {
    Bytes e = c.encode();
    w.write_bytes(codec::ByteView(e.data(), e.size()));
  });
  return w.take();
}

Anchor Anchor::decode(codec::CodecReader& r) {
  Anchor a;
  a.child_chain_id = r.read_u64();
  a.headers = r.read_list<CertifiedHeader>([](codec::CodecReader& r) {
    Bytes e = r.read_bytes();
    return CertifiedHeader::decode_all(codec::ByteView(e.data(), e.size()));
  });
  return a;
}

Anchor Anchor::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Anchor a = decode(r);
  r.assert_fully_consumed();
  return a;
}

Hash Anchor::hash() const { return crypto::hash_tagged(crypto::Domain::kAnchor, encode()); }

Hash compute_anchor_root(const std::vector<Anchor>& anchors) {
  if (anchors.empty()) return crypto::zero_hash();
  std::vector<Hash> leaves;
  leaves.reserve(anchors.size());
  for (const auto& a : anchors) leaves.push_back(a.hash());
  return merkle::compute_root(leaves);
}

BlockHeader BlockHeader::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  BlockHeader h = decode(r);
  r.assert_fully_consumed();
  return h;
}

Hash BlockHeader::hash() const {
  return crypto::hash_tagged(crypto::Domain::kHeader, encode());
}

// -- Block --------------------------------------------------------------------

Bytes Block::encode() const {
  if (header.version < 3 && !anchors.empty()) {
    throw std::invalid_argument("Block::encode: anchors require a version-3 header");
  }
  codec::CodecWriter w;
  Bytes hdr = header.encode();
  w.write_bytes(view(hdr));
  w.write_list(transactions,
               [](codec::CodecWriter& w, const Transaction& tx) {
                 Bytes e = tx.encode();
                 w.write_bytes(codec::ByteView(e.data(), e.size()));
               });
  if (header.version >= 3) {
    w.write_list(anchors, [](codec::CodecWriter& w, const Anchor& a) {
      Bytes e = a.encode();
      w.write_bytes(codec::ByteView(e.data(), e.size()));
    });
  }
  return w.take();
}

Block Block::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Block b;
  Bytes hdr = r.read_bytes();
  b.header = BlockHeader::decode_all(codec::ByteView(hdr.data(), hdr.size()));
  b.transactions = r.read_list<Transaction>([](codec::CodecReader& r) {
    Bytes e = r.read_bytes();
    return Transaction::decode_all(codec::ByteView(e.data(), e.size()));
  });
  if (b.header.version >= 3) {
    b.anchors = r.read_list<Anchor>([](codec::CodecReader& r) {
      Bytes e = r.read_bytes();
      return Anchor::decode_all(codec::ByteView(e.data(), e.size()));
    });
  }
  r.assert_fully_consumed();
  return b;
}

// -- Receipt ------------------------------------------------------------------

Bytes Receipt::encode() const {
  codec::CodecWriter w;
  w.write_u8(status);
  w.write_u128(fee);
  return w.take();
}

Receipt Receipt::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  Receipt rc;
  rc.status = r.read_u8();
  rc.fee = r.read_u128();
  r.assert_fully_consumed();
  return rc;
}

// -- Free functions -----------------------------------------------------------

Hash tx_leaf_hash(const Bytes& encoded) { return crypto::hash_tagged(crypto::Domain::kLeaf, encoded); }

Hash tx_root_from_leaves(const std::vector<Hash>& leaves) { return merkle::compute_root(leaves); }

Hash compute_tx_root(const std::vector<Transaction>& txs) {
  std::vector<Hash> leaves;
  leaves.reserve(txs.size());
  for (const auto& tx : txs) leaves.push_back(tx_leaf_hash(tx.encode()));
  return tx_root_from_leaves(leaves);
}

Hash compute_receipt_root(const std::vector<Receipt>& receipts) {
  std::vector<Hash> leaves;
  leaves.reserve(receipts.size());
  for (const auto& rc : receipts) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, rc.encode()));
  }
  return merkle::compute_root(leaves);
}

void sign_transaction(Transaction& tx, const crypto::KeyPair& kp) {
  // The caller sets version + sig_algo first; author is committed to by
  // signing_bytes(), so set it before computing the message.
  tx.author = kp.public_key;
  Bytes msg = tx.signing_bytes();
  if (tx.algo() == SigAlgo::kHybridEd25519MlDsa) {
    tx.signature = crypto::hybrid_sig_sign(view(msg), view(kp.secret_key));
  } else if (tx.algo() == SigAlgo::kSlhDsa) {
    tx.signature = crypto::slh_dsa_sign(view(msg), view(kp.secret_key));
  } else {
    tx.signature = crypto::ed25519_sign(view(msg), view(kp.secret_key));
  }
}

bool verify_transaction_signature(const Transaction& tx) {
  const Bytes msg = tx.signing_bytes();
  switch (tx.algo()) {
    case SigAlgo::kEd25519:
      // Fixed sizes: reject malformed before touching the backend.
      if (tx.author.size() != 32 || tx.signature.size() != 64) return false;
      return crypto::ed25519_verify(view(msg), view(tx.signature), view(tx.author));
    case SigAlgo::kHybridEd25519MlDsa:
      // hybrid_sig_verify checks both the Ed25519 and ML-DSA parts; a truncated
      // (downgraded) signature or mismatched key fails.
      return crypto::hybrid_sig_verify(view(msg), view(tx.signature), view(tx.author));
    case SigAlgo::kUtxoWitness:
      // Per-input witnesses are verified by the UTXO state machine's validate()
      // (which knows each input's owner). The account model rejects this scheme
      // (empty author), so it is only ever honoured on a UTXO chain. Envelope
      // verification is therefore a pass-through here.
      return true;
    case SigAlgo::kSlhDsa:
      // Fixed SLH-DSA-SHA2-128s shapes: pk 32 B, signature 7856 B. Reject
      // malformed sizes before touching the backend.
      if (tx.author.size() != 32 || tx.signature.size() != 7856) return false;
      return crypto::slh_dsa_verify(view(msg), view(tx.signature), view(tx.author));
    default:
      return false;  // unknown scheme - never accept
  }
}

}  // namespace aviary
