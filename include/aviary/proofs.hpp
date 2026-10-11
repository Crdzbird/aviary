#pragma once

#include <cstdint>

#include "aviary/codec.hpp"
#include "aviary/crypto.hpp"
#include "aviary/merkle.hpp"
#include "aviary/types.hpp"

/// Inclusion proofs (ARCHITECTURE section 10). A light client that has verified a
/// header (via the commit certificate) trusts its tx_root and state_root, and
/// can then verify these proofs against those roots - without full data.
///
/// M5 supports **presence** proofs. Absence proofs (a key is NOT in the state)
/// need neighbor proofs over a sorted trie and are deferred (see IMPROVEMENTS).
namespace aviary {

/// Proof that a state entry (account or UTXO) is committed under a state_root.
/// The leaf hash is hash_tagged(kLeaf, leaf_preimage); the verifier may also
/// decode leaf_preimage to read the committed value.
struct StateProof {
  bool present = false;
  Bytes leaf_preimage;
  std::uint32_t leaf_count = 0;
  merkle::Proof proof;

  Bytes encode() const;
  static StateProof decode_all(codec::ByteView bytes);
};

/// Verify a presence proof against [state_root]. Returns false for an
/// absence (present=false) proof - absence verification is not yet supported.
bool verify_state_proof(const Hash& state_root, const StateProof& p);

/// Proof that a transaction is included in a block, against the block's tx_root.
struct TxInclusionProof {
  Bytes tx_encoding;  ///< leaf = hash_tagged(kLeaf, tx_encoding)
  std::uint32_t leaf_count = 0;
  merkle::Proof proof;

  Bytes encode() const;
  static TxInclusionProof decode_all(codec::ByteView bytes);
};

/// Build an inclusion proof for transaction [index] of [block].
TxInclusionProof tx_inclusion_proof(const Block& block, std::uint32_t index);

/// Verify a tx-inclusion proof against [tx_root].
bool verify_tx_inclusion(const Hash& tx_root, const TxInclusionProof& p);

/// Proof that an execution receipt is committed under a block's receipt_root
/// (version-2 headers).
struct ReceiptProof {
  Bytes receipt_encoding;  ///< leaf = hash_tagged(kLeaf, receipt_encoding)
  std::uint32_t leaf_count = 0;
  merkle::Proof proof;

  Bytes encode() const;
  static ReceiptProof decode_all(codec::ByteView bytes);
};

/// Build an inclusion proof for receipt [index] within [receipts] (block order).
ReceiptProof receipt_inclusion_proof(const std::vector<Receipt>& receipts, std::uint32_t index);

/// Verify a receipt-inclusion proof against [receipt_root].
bool verify_receipt_inclusion(const Hash& receipt_root, const ReceiptProof& p);

}  // namespace aviary
