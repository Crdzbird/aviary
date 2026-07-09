#pragma once

#include <cstdint>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"

/// Core ledger data model (ARCHITECTURE §6). Model-agnostic: the [Transaction]
/// is a signed transport envelope whose [body] is interpreted by the pluggable
/// StateMachine (account or UTXO). Everything here is canonically encoded.
namespace blockchainer {

using crypto::Hash;
using Bytes = std::vector<std::uint8_t>;

/// Genesis-selected ledger model.
enum class StateModel : std::uint8_t { kAccount = 0, kUtxo = 1 };

/// A signed transaction. The signature (Ed25519) covers [signing_bytes]
/// (everything but the signature itself). [chain_id] is bound in, giving
/// cross-chain replay protection.
/// Transaction signature schemes. Serialized as `sig_algo` only in the extended
/// (version >= 2) envelope; version 1 is implicitly kEd25519.
enum class SigAlgo : std::uint8_t {
  kEd25519 = 1,             ///< classical Ed25519 (author 32 B, signature 64 B)
  kHybridEd25519MlDsa = 2,  ///< Ed25519 + ML-DSA-65 (concatenated pub / sig blobs)
  kUtxoWitness = 3,         ///< multi-signer UTXO: author empty, per-input witnesses
                            ///< in [signature]; verification is done by the UTXO
                            ///< state machine (one signer per input).
  kSlhDsa = 4,              ///< SLH-DSA-SHA2-128s (FIPS 205, stateless hash-based).
                            ///< Security rests on SHA-2 alone — no lattice
                            ///< assumption — so it survives even if ML-DSA falls.
                            ///< author 32 B, signature 7856 B.
};

struct Transaction {
  /// 1 = legacy Ed25519 envelope (no sig_algo byte); 2 = extended envelope that
  /// carries [sig_algo]. Bumping the version — not adding an unconditional field
  /// — keeps every version-1 canonical encoding (and its golden vectors) intact.
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  /// Signature scheme (a SigAlgo). Only serialized when version >= 2. Committed
  /// to by signing_bytes(), so an algorithm downgrade fails verification.
  std::uint8_t sig_algo = static_cast<std::uint8_t>(SigAlgo::kEd25519);
  /// Priority fee (tip) the sender offers, on top of any chain base fee. Only
  /// serialized when version >= 2 (version 1 pays no tip). Committed to by
  /// signing_bytes(), so it can't be tampered after signing. The mempool orders
  /// and evicts by this; the state machine burns it.
  codec::u128 fee = 0;
  Bytes body;       ///< model-specific canonical payload
  Bytes author;     ///< signer public key (32 B Ed25519, or hybrid pub blob)
  Bytes signature;  ///< signature over signing_bytes()

  /// The effective signature scheme: sig_algo for version >= 2, else kEd25519.
  SigAlgo algo() const {
    return version >= 2 ? static_cast<SigAlgo>(sig_algo) : SigAlgo::kEd25519;
  }

  /// The declared priority fee: fee for version >= 2, else 0.
  codec::u128 declared_fee() const { return version >= 2 ? fee : 0; }

  /// Canonical bytes the signature commits to (excludes [signature]).
  Bytes signing_bytes() const;
  /// Full canonical encoding (includes [signature]).
  Bytes encode() const;
  static Transaction decode(codec::CodecReader& r);
  static Transaction decode_all(codec::ByteView bytes);

  /// Transaction id: BLAKE3-256( 0x03 ‖ encode() ).
  Hash txid() const;
};

struct BlockHeader {
  /// 1 = base header; 2 = also commits [receipt_root]. Bumping the version — not
  /// adding an unconditional field — keeps every version-1 encoding (and its
  /// golden vectors) byte-identical.
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  std::uint64_t height = 0;
  Hash parent_hash{};
  std::uint64_t timestamp = 0;
  Hash tx_root{};
  Hash state_root{};
  Hash receipt_root{};   ///< only serialized when version >= 2
  Bytes consensus_data;  ///< opaque to the ledger; owned by the consensus engine

  Bytes encode() const;
  static BlockHeader decode(codec::CodecReader& r);
  static BlockHeader decode_all(codec::ByteView bytes);

  /// Header hash: BLAKE3-256( 0x02 ‖ encode() ).
  Hash hash() const;
};

struct Block {
  BlockHeader header;
  std::vector<Transaction> transactions;

  Bytes encode() const;
  static Block decode_all(codec::ByteView bytes);
};

/// A transaction's execution result, committed via a block's receipt_root
/// (version-2 headers). `status` is 1 for success; `fee` is what the tx paid.
struct Receipt {
  std::uint8_t status = 1;
  codec::u128 fee = 0;

  Bytes encode() const;
  static Receipt decode_all(codec::ByteView bytes);
};

/// Merkle root over hash_tagged(kLeaf, receipt.encode()) for each receipt, in order.
Hash compute_receipt_root(const std::vector<Receipt>& receipts);

/// Merkle root over hash_tagged(kLeaf, tx.encode()) for each tx, in order.
Hash compute_tx_root(const std::vector<Transaction>& txs);

/// Signs [tx] in place: sets [author] from [kp] and [signature] over
/// signing_bytes().
void sign_transaction(Transaction& tx, const crypto::KeyPair& kp);

/// Verifies [tx]'s signature against its embedded [author]. Never throws.
bool verify_transaction_signature(const Transaction& tx);

}  // namespace blockchainer
