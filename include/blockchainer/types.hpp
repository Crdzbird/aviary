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
struct Transaction {
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  Bytes body;       ///< model-specific canonical payload
  Bytes author;     ///< Ed25519 public key (32 B) — the signer
  Bytes signature;  ///< Ed25519 signature (64 B) over signing_bytes()

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
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  std::uint64_t height = 0;
  Hash parent_hash{};
  std::uint64_t timestamp = 0;
  Hash tx_root{};
  Hash state_root{};
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

/// Merkle root over hash_tagged(kLeaf, tx.encode()) for each tx, in order.
Hash compute_tx_root(const std::vector<Transaction>& txs);

/// Signs [tx] in place: sets [author] from [kp] and [signature] over
/// signing_bytes().
void sign_transaction(Transaction& tx, const crypto::KeyPair& kp);

/// Verifies [tx]'s signature against its embedded [author]. Never throws.
bool verify_transaction_signature(const Transaction& tx);

}  // namespace blockchainer
