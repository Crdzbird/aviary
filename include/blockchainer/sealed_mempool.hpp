#pragma once

#include <cstdint>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// Encrypted mempool (ARCHITECTURE §9, anti-MEV): transactions are sealed to the
/// block proposer's key and stay opaque in the pool until the proposer opens
/// them at block-build time. A front-runner watching the mempool sees only
/// ciphertext plus a fee hint — never the transaction's contents (which
/// contract, which amounts) — so it cannot profitably re-order or sandwich.
///
/// The submitter seals the encoded transaction to the proposer's hybrid-KEM
/// public key (recipient-bound PQ seal); only the proposer's KEM secret opens
/// it. A cleartext `fee_hint` rides alongside so the pool can order by fee
/// WITHOUT decrypting; it is bound as the seal's AAD, so it can't be swapped
/// after sealing, and on reveal the proposer additionally checks the opened
/// transaction's real fee equals the hint — a submitter can't buy priority with
/// a lie. This is a submission envelope only: committed blocks carry ordinary
/// plaintext transactions, so nothing downstream (consensus, ledger, wire)
/// changes.
namespace blockchainer::p2p {

/// A transaction sealed to a proposer, with a public fee hint for ordering.
struct SealedTx {
  codec::u128 fee_hint = 0;  ///< cleartext (for ordering); AAD-bound + checked on open
  Bytes envelope;            ///< pq_seal_to(tx.encode(), proposer_kem_pub, aad = fee_hint)

  Bytes encode() const;
  static SealedTx decode(codec::ByteView bytes);

  /// Seal [tx] to [proposer_kem_public]. The hint is the tx's declared fee.
  static SealedTx seal(const Transaction& tx, crypto::ByteView proposer_kem_public);
};

class SealedMempool {
 public:
  explicit SealedMempool(std::size_t capacity = 4096) : capacity_(capacity) {}

  /// Admit a sealed submission (no decryption). Rejects only when full. Kept in
  /// fee-hint order (highest first); ties broken by arrival for determinism.
  bool add(const SealedTx& tx);

  std::size_t size() const { return pool_.size(); }

  /// Proposer-only: open up to [max] of the highest-fee submissions with
  /// [proposer_kem_secret], returning the transactions that (a) decrypt, (b)
  /// decode to a valid transaction, and (c) whose real declared fee equals the
  /// committed hint. Undecryptable / malformed / fee-lying entries are silently
  /// skipped (they were never really in the auction). Const — revealing does not
  /// drain the pool; the caller removes what it includes.
  std::vector<Transaction> reveal(crypto::ByteView proposer_kem_secret, std::size_t max) const;

  /// Drop submissions by the exact sealed-envelope bytes (e.g. after inclusion).
  void remove(const std::vector<Bytes>& envelopes);

 private:
  std::size_t capacity_;
  std::vector<SealedTx> pool_;  // maintained in descending fee-hint order
};

}  // namespace blockchainer::p2p
