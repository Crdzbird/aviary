#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "aviary/consensus.hpp"
#include "aviary/crypto.hpp"
#include "aviary/merkle.hpp"
#include "aviary/proofs.hpp"
#include "aviary/subchain.hpp"
#include "aviary/types.hpp"

/// Subchains version 2 - docs/design/SUBCHAINS.md Part 2 (sections 13 to 26).
/// A parent whose genesis commits a DynamicSubchainPolicy registers children
/// in any block through signed registry operations, commits its registry as a
/// Merkle tree with one record per child, and can close a child. Blocks of such
/// a parent are header version 4. The version 1 policy, registry and proofs of
/// subchain.hpp are untouched and remain what a version 3 parent uses.
namespace aviary {
struct GenesisSpec;

/// A key allowed to authorise registry operations: a transaction signature
/// scheme (SigAlgo 1 Ed25519, 2 hybrid Ed25519 plus ML-DSA-65, 4 SLH-DSA) and
/// its public key.
struct Registrar {
  std::uint8_t sig_algo = 0;
  Bytes public_key;
  bool operator==(const Registrar&) const = default;
};

/// The registrar keys of a parent and how many of them must approve an
/// operation. Part of the genesis policy, replaceable by operation kind 3,
/// committed into every version-4 subchain_root through hash().
struct RegistrarSet {
  /// The most registrars a set may hold.
  static constexpr std::size_t kMaxRegistrars = 64;

  std::uint8_t threshold = 0;
  std::vector<Registrar> registrars;  ///< index significant, unique

  /// u8 threshold || list(u8 sig_algo || bytes(public_key)).
  Bytes encode() const;
  static RegistrarSet decode(codec::CodecReader& r);
  static RegistrarSet decode_all(crypto::ByteView bytes);
  /// BLAKE3-256( kRegistrarSet || encode() ).
  crypto::Hash hash() const;
  /// Empty string = valid: at most kMaxRegistrars, known schemes, key sizes
  /// that fit the scheme, no repeated key, and a threshold of 0 for an empty
  /// set or 1..n otherwise.
  std::string validate() const;
  bool operator==(const RegistrarSet&) const = default;
};

/// Which policy format a genesis `subchain_policy` blob is: 0 for an empty
/// blob (not a parent), 1 for the version 1 SubchainPolicy, 2 for a
/// DynamicSubchainPolicy, and any other value for a blob that carries the
/// dynamic escape prefix with a format this build does not know.
std::uint8_t subchain_policy_format(crypto::ByteView policy);

/// The parent side policy of version 2, committed into the parent's genesis
/// hash in the same genesis field as the version 1 policy.
struct DynamicSubchainPolicy {
  std::uint8_t max_depth = 1;                ///< levels allowed below this chain
  std::uint32_t max_children = 0;            ///< bound on OPEN children; 0 = no policy bound
  std::uint16_t max_anchors_per_block = 0;   ///< 0 = unlimited
  std::uint16_t max_headers_per_anchor = 0;  ///< 0 = unlimited
  std::uint16_t max_ops_per_block = 0;       ///< 0 = unlimited
  RegistrarSet authority;                    ///< the first registrar set; empty = sealed registry
  std::vector<ChildRegistration> children;   ///< registered at genesis; ascending, unique chain_id

  /// 00 00 00 || u8 2 || u8 max_depth || u32 max_children || u16 max_anchors_per_block ||
  /// u16 max_headers_per_anchor || u16 max_ops_per_block || RegistrarSet || list(bytes(ChildRegistration)).
  Bytes encode() const;
  static DynamicSubchainPolicy decode_all(crypto::ByteView bytes);
  crypto::Hash hash() const;
  /// Structural validity; empty string = valid (section 14 of the design).
  std::string validate(std::uint64_t own_chain_id) const;

  void commit_to(GenesisSpec& g) const;
  /// The policy of [g], or nullopt when [g] carries none or a version 1 policy.
  static std::optional<DynamicSubchainPolicy> from_genesis(const GenesisSpec& g);
  bool operator==(const DynamicSubchainPolicy&) const = default;
};

/// The body of a close operation: which child, and the tip that is final.
struct CloseChild {
  std::uint64_t chain_id = 0;
  std::uint64_t final_height = 0;
  Hash final_tip_hash{};

  Bytes encode() const;
  static CloseChild decode_all(crypto::ByteView bytes);
  bool operator==(const CloseChild&) const = default;
};

/// An unsigned operation registering [registration], valid in [authority_epoch]
/// up to parent height [valid_until] (0 = no expiry).
SubchainOp make_register_op(std::uint32_t authority_epoch, std::uint64_t valid_until,
                            const ChildRegistration& registration);
/// An unsigned operation closing a child at the tip [close] names.
SubchainOp make_close_op(std::uint32_t authority_epoch, std::uint64_t valid_until, const CloseChild& close);
/// An unsigned operation replacing the registrar set with [next].
SubchainOp make_set_registrars_op(std::uint32_t authority_epoch, std::uint64_t valid_until,
                                  const RegistrarSet& next);

/// Add registrar [registrar_index]'s approval with a signature produced
/// elsewhere, keeping approvals ascending. Throws std::invalid_argument if
/// that registrar already approved.
void add_op_approval(SubchainOp& op, std::uint16_t registrar_index, Bytes signature);
/// Sign [op]'s authorisation digest for the parent (chain id, genesis hash)
/// with [secret_key] under [algo] and add the approval.
void approve_op(SubchainOp& op, std::uint64_t parent_chain_id, const Hash& parent_genesis_hash,
                std::uint16_t registrar_index, SigAlgo algo, crypto::ByteView secret_key);

/// One child's entry in a dynamic registry: the Merkle leaf preimage and the
/// unit a proof carries. Fixed 230 byte encoding.
struct ChildRecord {
  static constexpr std::uint8_t kOpen = 1;
  static constexpr std::uint8_t kClosed = 2;
  static constexpr std::size_t kEncodedSize = 230;

  std::uint32_t index = 0;              ///< registration order = leaf position
  std::uint64_t chain_id = 0;
  std::uint8_t status = kOpen;
  std::uint8_t child_max_depth = 0;
  Hash genesis_header_hash{};
  std::uint64_t registered_height = 0;  ///< parent height of the registering block; 0 = parent genesis
  std::uint64_t closed_height = 0;      ///< 0 while open
  std::uint64_t tip_height = 0;
  Hash tip_hash{};
  Hash tip_state_root{};
  Hash tip_subchain_root{};             ///< the child's own registry root; zero unless it is a parent
  Hash vset_hash{};                     ///< the child's validator set in force for its next header
  Hash tip_vset_hash{};                 ///< the set that certified the tip

  Bytes encode() const;
  /// Strict: status must be kOpen or kClosed, and closed_height is zero
  /// exactly when the record is open.
  static ChildRecord decode_all(crypto::ByteView bytes);
  /// BLAKE3-256( kSubchainLeaf || encode() ).
  Hash leaf() const;
  bool operator==(const ChildRecord&) const = default;
};

/// subchain_root of a version-4 header: BLAKE3-256( kDynamicRegistry ||
/// u32 record_count || u32 open_count || u32 authority_epoch ||
/// authority_hash || children_root ).
Hash dynamic_registry_root(std::uint32_t record_count, std::uint32_t open_count, std::uint32_t authority_epoch,
                           const Hash& authority_hash, const Hash& children_root);

/// Proof that one child's record is committed by a version-4 subchain_root:
/// the record, the outer fields of the root, and the record's Merkle path.
struct ChildProof {
  ChildRecord record;
  std::uint32_t record_count = 0;
  std::uint32_t open_count = 0;
  std::uint32_t authority_epoch = 0;
  Hash authority_hash{};
  std::vector<Hash> siblings;  ///< bottom up

  Bytes encode() const;
  static ChildProof decode_all(crypto::ByteView bytes);
  bool operator==(const ChildProof&) const = default;
};

/// The record [proof] commits under [subchain_root], or nullopt when the proof
/// does not hold. The caller reads the record: its tip, and whether the child
/// is open. Never throws.
std::optional<ChildRecord> verify_child_proof(const Hash& subchain_root, const ChildProof& proof);

/// A cross-chain proof through any mixture of version 3 and version 4 parents:
/// hop by hop from a parent's subchain_root down to a state entry.
struct SubchainProofV2 {
  static constexpr std::uint8_t kFlatHop = 1;    ///< body = a SubchainProof::Hop (version 3 parent)
  static constexpr std::uint8_t kRecordHop = 2;  ///< body = a ChildProof (version 4 parent)
  struct Hop {
    std::uint8_t kind = 0;
    Bytes body;
    bool operator==(const Hop&) const = default;
  };
  std::vector<Hop> hops;
  StateProof state_proof;  ///< against the last hop's child tip state_root

  /// list(u8 kind || bytes(body)) || bytes(StateProof).
  Bytes encode() const;
  static SubchainProofV2 decode_all(crypto::ByteView bytes);
};
/// True iff every hop is committed under the root reached so far, starting at
/// [parent_subchain_root], and the state proof holds under the last child's
/// tip. A closed child's record still verifies (its final tip stays provable).
/// Never throws.
bool verify_subchain_proof_v2(const Hash& parent_subchain_root, const SubchainProofV2& proof);

/// The parent's live registry under a dynamic policy. apply() validates and
/// applies one block's operations then anchors (design section 15), purely as
/// a function of (registry, height, operations, anchors).
///
/// Copies are cheap: they share an immutable base and carry only the records a
/// block touched, so the Ledger validates on a copy and commits or drops it.
/// compact() folds a copy's changes into the base, in place when no other copy
/// shares it.
class DynamicSubchainRegistry {
 public:
  struct Child {
    ChildRecord record;
    BlockHeader tip;  ///< last anchored header (the genesis header until anchored)
    std::shared_ptr<const consensus::ValidatorSet> vset;      ///< in force next; null once closed
    std::shared_ptr<const consensus::ValidatorSet> tip_vset;  ///< certified [tip]; null once closed
  };

  /// Build from a policy. Throws std::invalid_argument on a policy that fails
  /// validate, a malformed genesis child validator set, or an all-zero
  /// [own_genesis_hash] (operations are bound to it).
  static DynamicSubchainRegistry from_policy(std::uint64_t own_chain_id, const Hash& own_genesis_hash,
                                             DynamicSubchainPolicy policy);

  const DynamicSubchainPolicy& policy() const { return *policy_; }
  std::uint32_t record_count() const;
  std::uint32_t open_count() const { return open_count_; }
  std::uint32_t authority_epoch() const { return epoch_; }
  const RegistrarSet& authority() const { return *authority_; }

  /// The child registered under [chain_id], open or closed, or nullptr. The
  /// pointer is valid until this registry is next modified.
  const Child* child(std::uint64_t chain_id) const;
  /// The child at record [index], or nullptr.
  const Child* child_at(std::uint32_t index) const;
  /// The last anchored header of [chain_id], or nullptr if not registered.
  const BlockHeader* tip_of(std::uint64_t chain_id) const;

  /// Validate and apply a block's [ops] then [anchors] at parent [height].
  /// Returns "" on success; on failure returns the reason and THIS COPY is
  /// unspecified - discard it.
  std::string apply(std::uint64_t height, const std::vector<SubchainOp>& ops, const std::vector<Anchor>& anchors);

  /// The version-4 subchain_root of the registry as it stands.
  Hash root() const;
  /// The proof for [chain_id] under root(). Throws std::invalid_argument if it
  /// was never registered.
  ChildProof prove(std::uint64_t chain_id) const;

  /// The whole registry for a snapshot (design section 22).
  Bytes serialize() const;
  /// Replace the contents with a snapshot. Throws on bytes that do not decode
  /// strictly or are inconsistent (duplicate ids, status fields, a registrar
  /// set that fails validate). The caller compares root() with the tip header.
  void restore(crypto::ByteView bytes);

  /// Fold pending changes into the base.
  void compact();

 private:
  struct Base {
    std::vector<Child> children;
    std::unordered_map<std::uint64_t, std::uint32_t> index_of;
    merkle::AppendTree tree;
  };

  Child& touch(std::uint32_t index);
  merkle::AppendTree::Batch batch() const;
  std::string apply_op(std::uint64_t height, const SubchainOp& op);
  std::string apply_anchors(const std::vector<Anchor>& anchors);

  std::shared_ptr<const DynamicSubchainPolicy> policy_;
  std::uint64_t own_chain_id_ = 0;
  Hash own_genesis_hash_{};
  std::shared_ptr<Base> base_;
  std::map<std::uint32_t, Child> changed_;
  std::vector<Child> appended_;
  std::unordered_map<std::uint64_t, std::uint32_t> appended_index_;
  std::shared_ptr<const RegistrarSet> authority_;
  Hash authority_hash_{};
  std::uint32_t epoch_ = 0;
  std::uint32_t open_count_ = 0;
};

}  // namespace aviary
