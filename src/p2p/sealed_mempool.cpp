#include "aviary/sealed_mempool.hpp"

#include <algorithm>

namespace aviary::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// The AAD binding the cleartext fee hint to the ciphertext: the 16-byte u128.
Bytes fee_aad(codec::u128 fee) {
  codec::CodecWriter w;
  w.write_u128(fee);
  return w.take();
}
}  // namespace

Bytes SealedTx::encode() const {
  codec::CodecWriter w;
  w.write_u128(fee_hint);
  w.write_bytes(view(envelope));
  return w.take();
}

SealedTx SealedTx::decode(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  SealedTx s;
  s.fee_hint = r.read_u128();
  s.envelope = r.read_bytes();
  r.assert_fully_consumed();
  return s;
}

SealedTx SealedTx::seal(const Transaction& tx, crypto::ByteView proposer_kem_public) {
  SealedTx s;
  s.fee_hint = tx.declared_fee();
  const Bytes aad = fee_aad(s.fee_hint);
  s.envelope = crypto::pq_seal_to(view(tx.encode()), proposer_kem_public, view(aad));
  return s;
}

bool SealedMempool::add(const SealedTx& tx) {
  if (pool_.size() >= capacity_) {
    // Full: only admit if it outbids the current lowest hint (evict that one).
    if (pool_.empty() || tx.fee_hint <= pool_.back().fee_hint) return false;
    pool_.pop_back();
  }
  // Insert keeping descending fee-hint order; equal hints keep insertion order
  // (upper_bound on the "greater" comparator -> after equals) for determinism.
  auto pos = std::upper_bound(pool_.begin(), pool_.end(), tx,
                              [](const SealedTx& a, const SealedTx& b) { return a.fee_hint > b.fee_hint; });
  pool_.insert(pos, tx);
  return true;
}

std::vector<Transaction> SealedMempool::reveal(crypto::ByteView proposer_kem_secret,
                                               std::size_t max) const {
  std::vector<Transaction> out;
  for (const SealedTx& s : pool_) {
    if (out.size() >= max) break;
    Bytes plain;
    try {
      plain = crypto::pq_open(view(s.envelope), proposer_kem_secret, view(fee_aad(s.fee_hint)));
    } catch (...) {
      continue;  // not for us / tampered - never really in the auction
    }
    Transaction tx;
    try {
      tx = Transaction::decode_all(view(plain));
    } catch (...) {
      continue;  // decrypted to garbage
    }
    // The submitter can't buy priority with a lie: the real fee must equal the
    // hint it was ordered by (the AAD already pins the hint to this ciphertext).
    if (tx.declared_fee() != s.fee_hint) continue;
    out.push_back(std::move(tx));
  }
  return out;
}

void SealedMempool::remove(const std::vector<Bytes>& envelopes) {
  for (const Bytes& e : envelopes) {
    auto it = std::find_if(pool_.begin(), pool_.end(),
                           [&](const SealedTx& s) { return s.envelope == e; });
    if (it != pool_.end()) pool_.erase(it);
  }
}

}  // namespace aviary::p2p
