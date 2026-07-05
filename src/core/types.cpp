#include "blockchainer/types.hpp"

#include "blockchainer/merkle.hpp"

namespace blockchainer {

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

// ── Transaction ──────────────────────────────────────────────────────────────

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

// ── BlockHeader ──────────────────────────────────────────────────────────────

Bytes BlockHeader::encode() const {
  codec::CodecWriter w;
  w.write_u8(version);
  w.write_u64(chain_id);
  w.write_u64(height);
  w.write_fixed_bytes(view(parent_hash), 32);
  w.write_u64(timestamp);
  w.write_fixed_bytes(view(tx_root), 32);
  w.write_fixed_bytes(view(state_root), 32);
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
  h.consensus_data = r.read_bytes();
  return h;
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

// ── Block ────────────────────────────────────────────────────────────────────

Bytes Block::encode() const {
  codec::CodecWriter w;
  Bytes hdr = header.encode();
  w.write_bytes(view(hdr));
  w.write_list(transactions,
               [](codec::CodecWriter& w, const Transaction& tx) {
                 Bytes e = tx.encode();
                 w.write_bytes(codec::ByteView(e.data(), e.size()));
               });
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
  r.assert_fully_consumed();
  return b;
}

// ── Free functions ───────────────────────────────────────────────────────────

Hash compute_tx_root(const std::vector<Transaction>& txs) {
  std::vector<Hash> leaves;
  leaves.reserve(txs.size());
  for (const auto& tx : txs) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf, tx.encode()));
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
    default:
      return false;  // unknown scheme — never accept
  }
}

}  // namespace blockchainer
