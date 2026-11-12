#pragma once

#include <stdexcept>
#include <cstdint>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/crypto.hpp"

/// Core ledger data model (ARCHITECTURE section 6). Model-agnostic: the [Transaction]
/// is a signed transport envelope whose [body] is interpreted by the pluggable
/// StateMachine (account or UTXO). Everything here is canonically encoded.
namespace aviary {

using crypto::Hash;
using Bytes = std::vector<std::uint8_t>;

/// Genesis-selected ledger model.
enum class StateModel : std::uint8_t { kAccount = 0, kUtxo = 1, kRecord = 2 };

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
                            ///< Security rests on SHA-2 alone - no lattice
                            ///< assumption - so it survives even if ML-DSA falls.
                            ///< author 32 B, signature 7856 B.
};

struct Transaction {
  /// 1 = legacy Ed25519 envelope (no sig_algo byte); 2 = extended envelope that
  /// carries [sig_algo]. Bumping the version - not adding an unconditional field
  /// - keeps every version-1 canonical encoding (and its golden vectors) intact.
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

  /// Transaction id: BLAKE3-256( 0x03 || encode() ).
  Hash txid() const;
};

struct BlockHeader {
  /// 1 = base header; 2 = also commits [receipt_root]; 3 = also commits
  /// [anchor_root] + [subchain_root] (docs/design/SUBCHAINS.md); 4 = also
  /// commits [ops_root] and its subchain_root is the Merkle registry of Part 2
  /// of that document. Bumping the version - not adding an unconditional
  /// field - keeps every lower version's encoding (and its golden vectors)
  /// byte-identical.
  std::uint8_t version = 1;
  std::uint64_t chain_id = 0;
  std::uint64_t height = 0;
  Hash parent_hash{};
  std::uint64_t timestamp = 0;
  Hash tx_root{};
  Hash state_root{};
  Hash receipt_root{};   ///< only serialized when version >= 2
  Hash anchor_root{};    ///< version >= 3: Merkle root of this block's subchain anchors
  Hash subchain_root{};  ///< version >= 3: commitment to the post-block subchain registry
  Hash ops_root{};       ///< version >= 4: Merkle root of this block's registry operations
  Bytes consensus_data;  ///< opaque to the ledger; owned by the consensus engine

  Bytes encode() const;
  static BlockHeader decode(codec::CodecReader& r);
  static BlockHeader decode_all(codec::ByteView bytes);

  /// Header hash: BLAKE3-256( 0x02 || encode() ).
  Hash hash() const;
  bool operator==(const BlockHeader&) const = default;
};

/// A child chain's finalized header with the certificate that finalized it
/// (the child's canonical `consensus::CommitCertificate` bytes - kept opaque
/// here so types.hpp stays free of the consensus layer; the subchain registry
/// decodes and verifies it). See docs/design/SUBCHAINS.md.
struct CertifiedHeader {
  BlockHeader header;
  Bytes certificate;

  Bytes encode() const;
  static CertifiedHeader decode(codec::CodecReader& r);
  static CertifiedHeader decode_all(codec::ByteView bytes);
  bool operator==(const CertifiedHeader&) const = default;
};

/// A subchain ANCHOR: one child chain's finalized progress, carried in a parent
/// block. [headers] is a CONSECUTIVE segment (child tip+1 ... tip+k, each with its
/// certificate), so the parent verifies every child header exactly as a light
/// client would - no gaps, no new verification rule. Bounded by the parent's
/// SubchainPolicy. Anchors in a block are ordered by ascending child_chain_id,
/// one per child.
struct Anchor {
  std::uint64_t child_chain_id = 0;
  std::vector<CertifiedHeader> headers;

  Bytes encode() const;
  static Anchor decode(codec::CodecReader& r);
  static Anchor decode_all(codec::ByteView bytes);
  /// BLAKE3-256( kAnchor || encode() ) - the Merkle leaf under anchor_root.
  Hash hash() const;
  bool operator==(const Anchor&) const = default;
};

/// One registrar's approval of a registry operation: which registrar (its
/// index in the parent's current registrar set) and its signature over the
/// operation's authorisation digest.
struct OpApproval {
  std::uint16_t registrar_index = 0;
  Bytes signature;
  bool operator==(const OpApproval&) const = default;
};

/// A registry operation carried in a version-4 parent block
/// (docs/design/SUBCHAINS.md section 15): register a child, close a child, or
/// replace the registrar set. The payload is the canonical bytes of the
/// operation's body, kept opaque here so types.hpp stays free of the registry;
/// the dynamic registry decodes and validates it.
struct SubchainOp {
  static constexpr std::uint8_t kRegister = 1;
  static constexpr std::uint8_t kClose = 2;
  static constexpr std::uint8_t kSetRegistrars = 3;

  std::uint8_t kind = 0;
  std::uint32_t authority_epoch = 0;  ///< the registry epoch this operation is valid in
  std::uint64_t valid_until = 0;      ///< last parent height it may be applied at; 0 = no expiry
  Bytes payload;
  std::vector<OpApproval> approvals;  ///< ascending, unique registrar_index

  Bytes encode() const;
  /// Rejects approvals that are not ascending and unique by registrar index.
  static SubchainOp decode(codec::CodecReader& r);
  static SubchainOp decode_all(codec::ByteView bytes);
  /// BLAKE3-256( kSubchainOp || encode() ) - the Merkle leaf under ops_root.
  Hash hash() const;
  /// The 32 bytes a registrar signs: BLAKE3-256( kSubchainOpAuth ||
  /// parent_chain_id || parent_genesis_hash || kind || authority_epoch ||
  /// valid_until || bytes(payload) ). Approvals are not part of it.
  Hash auth_digest(std::uint64_t parent_chain_id, const Hash& parent_genesis_hash) const;
  bool operator==(const SubchainOp&) const = default;
};

struct Block {
  BlockHeader header;
  std::vector<Transaction> transactions;
  std::vector<Anchor> anchors;  ///< only serialized when header.version >= 3
  std::vector<SubchainOp> ops;  ///< only serialized when header.version >= 4

  /// Throws std::invalid_argument if [anchors] or [ops] is non-empty on a
  /// header whose version cannot carry them - never silently drops a
  /// commitment.
  Bytes encode() const;
  static Block decode_all(codec::ByteView bytes);
};

/// Merkle root over anchors (leaf = anchor.hash()); zero_hash for none.
Hash compute_anchor_root(const std::vector<Anchor>& anchors);

/// Merkle root over registry operations in block order (leaf = op.hash());
/// zero_hash for none.
Hash compute_ops_root(const std::vector<SubchainOp>& ops);

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
/// The tx root leaf of an encoded transaction: hash_tagged(kLeaf, encoded).
Hash tx_leaf_hash(const Bytes& encoded);
/// The tx root over leaves from tx_leaf_hash, in block order.
Hash tx_root_from_leaves(const std::vector<Hash>& leaves);

/// Signs [tx] in place: sets [author] from [kp] and [signature] over
/// signing_bytes().
void sign_transaction(Transaction& tx, const crypto::KeyPair& kp);

/// Verifies [tx]'s signature against its embedded [author]. Never throws.
bool verify_transaction_signature(const Transaction& tx);

/// Verifies [signature] over [msg] under [public_key] with a key scheme
/// (kEd25519, kHybridEd25519MlDsa or kSlhDsa), applying the fixed size checks
/// of that scheme first. False for any other scheme. Never throws.
bool verify_signature(SigAlgo algo, codec::ByteView msg, codec::ByteView signature,
                      codec::ByteView public_key);

/// Signs [msg] with a key scheme (kEd25519, kHybridEd25519MlDsa or kSlhDsa).
/// Throws std::invalid_argument for any other scheme.
Bytes sign_message(SigAlgo algo, codec::ByteView msg, codec::ByteView secret_key);

}  // namespace aviary
