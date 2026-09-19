#pragma once

#include <optional>
#include <string>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/types.hpp"

/// Subchains — docs/design/SUBCHAINS.md. A parent chain ANCHORS its children:
/// a parent block carries each child's finalized headers + certificates, the
/// parent's validators verify them exactly as a light client would, and the
/// child's new tip is recorded in a registry committed into the parent header.
/// The parent never executes the child.
///
/// This header holds the genesis-committed POLICY (parent side), the child's
/// BINDING to its parent, and the REGISTRY types. Stage 1 = types + canonical
/// codec only; validation lives in the registry (stage 2).
namespace blockchainer {
struct GenesisSpec;

/// A child registered at the parent's genesis: its trusted starting header (the
/// child's GENESIS HEADER — which commits its chain id, genesis state root and,
/// if the child is itself a parent, its own initial registry), its FIXED
/// validator set, and how deep its own subtree may go.
///
/// Why the genesis HEADER and not the genesis HASH: the child's genesis hash
/// commits its ParentBinding (the parent's genesis hash), and the parent's
/// genesis hash commits this registration — a cycle. The genesis header does
/// not depend on the binding, so parent and child can each commit to the other.
struct ChildRegistration {
  std::uint64_t chain_id = 0;
  BlockHeader genesis_header;
  std::vector<consensus::Validator> validators;
  std::uint8_t child_max_depth = 0;  ///< the child's own policy depth (0 = leaf); I10

  Bytes encode() const;
  static ChildRegistration decode(codec::CodecReader& r);
  bool operator==(const ChildRegistration&) const = default;
};

/// The parent-side policy, committed into the parent's genesis hash.
struct SubchainPolicy {
  std::uint16_t max_children = 0;          ///< 0 = this chain anchors nothing
  std::uint8_t max_depth = 1;              ///< levels allowed below this chain
  std::uint16_t max_anchors_per_block = 0; ///< 0 = unlimited (bounded by children)
  std::uint16_t max_headers_per_anchor = 0;///< 0 = unlimited
  std::vector<ChildRegistration> children; ///< ascending, unique chain_id

  Bytes encode() const;
  static SubchainPolicy decode_all(crypto::ByteView bytes);
  crypto::Hash hash() const;
  /// Structural validity: sorted unique children, count ≤ max_children, each
  /// child's chain_id ≠ [own_chain_id], non-empty validator sets, genesis
  /// header at height 0 with matching chain_id, and each child's subtree fits
  /// (child_max_depth + 1 ≤ max_depth). Empty string = valid.
  std::string validate(std::uint64_t own_chain_id) const;

  void commit_to(GenesisSpec& g) const;
  static std::optional<SubchainPolicy> from_genesis(const GenesisSpec& g);
  bool operator==(const SubchainPolicy&) const = default;
};

/// One child's line in a registry commitment; a `SubchainProof` carries the
/// whole (bounded) list so a verifier recomputes `subchain_root` exactly.
struct ChildSummary {
  std::uint64_t chain_id = 0;
  Hash genesis_header_hash{};
  BlockHeader tip;
  bool operator==(const ChildSummary&) const = default;
};
/// `subchain_root` over an ascending summary list (the registry uses this too).
Hash registry_root(const std::vector<ChildSummary>& summaries);

/// A cross-chain proof: from a parent's `subchain_root`, hop by hop down the
/// tree, to a state entry in the last chain. Verification (docs/design/
/// SUBCHAINS.md §7): for each hop recompute registry_root(summaries) == the
/// current root, find `child_chain_id`, and continue with that child tip's own
/// subchain_root; after the last hop verify the state proof against the child
/// tip's state_root. The same function serves any depth — no new crypto.
struct SubchainProof {
  struct Hop {
    std::vector<ChildSummary> summaries;  ///< the parent's whole registry at that height
    std::uint64_t child_chain_id = 0;     ///< which child this hop descends into
    Bytes encode() const;
    static Hop decode_all(crypto::ByteView bytes);
    bool operator==(const Hop&) const = default;
  };
  std::vector<Hop> hops;
  StateProof state_proof;  ///< against the final child tip's state_root

  Bytes encode() const;
  static SubchainProof decode_all(crypto::ByteView bytes);
};
/// True iff every hop is committed under [parent_subchain_root] and the state
/// proof holds under the final child tip. Never throws.
bool verify_subchain_proof(const Hash& parent_subchain_root, const SubchainProof& p);

/// The child-side binding, committed into the child's genesis hash.
struct ParentBinding {
  std::uint64_t parent_chain_id = 0;
  Hash parent_genesis_hash{};

  Bytes encode() const;
  static ParentBinding decode_all(crypto::ByteView bytes);
  crypto::Hash hash() const;
  void commit_to(GenesisSpec& g) const;
  static std::optional<ParentBinding> from_genesis(const GenesisSpec& g);
  bool operator==(const ParentBinding&) const = default;
};

/// The parent's live view of its children: for each registered child, the
/// last anchored (finalized) header. Anchor verification IS `LightClient::
/// accept` — each child header must be the next one (height +1, parent hash,
/// embedded last-commit) under the child's FIXED registered validator set, and
/// its certificate must reach quorum. Pure and deterministic: the result is a
/// function of (registry, anchors) only. Copy it, apply, and commit the copy
/// only on success (the Ledger does exactly that).
class SubchainRegistry {
 public:
  struct ChildEntry {
    ChildRegistration reg;
    consensus::ValidatorSet vset;  ///< built once from reg.validators
    BlockHeader tip;               ///< last anchored header (genesis until anchored)
  };

  /// Build from a validated policy (throws std::invalid_argument on a policy
  /// that fails `validate` or a malformed child validator set).
  static SubchainRegistry from_policy(std::uint64_t own_chain_id, SubchainPolicy policy);

  const SubchainPolicy& policy() const { return policy_; }
  const std::vector<ChildEntry>& children() const { return children_; }
  /// The last anchored header of [child], or nullptr if not registered.
  const BlockHeader* tip_of(std::uint64_t child_chain_id) const;

  /// Validate a block's anchors (docs/design/SUBCHAINS.md §6: count bound,
  /// canonical order, registered child, segment bound, then per header the
  /// light-client rule) and advance the tips. Returns "" on success; on
  /// failure returns the reason and THIS COPY is unspecified — discard it.
  std::string apply_anchors(const std::vector<Anchor>& anchors);

  /// `subchain_root` = registry_root(summaries()).
  Hash root() const;
  /// The registry as a summary list (what a proof carries).
  std::vector<ChildSummary> summaries() const;
  /// A proof hop descending into [child_chain_id] at the current registry
  /// state. Throws if the child is not registered.
  SubchainProof::Hop hop(std::uint64_t child_chain_id) const;

  /// The mutable part (child tips) for snapshots; the registrations come from
  /// the policy. `restore_tips` throws on a blob that does not match the policy.
  Bytes serialize_tips() const;
  void restore_tips(crypto::ByteView bytes);

 private:
  SubchainPolicy policy_;
  std::vector<ChildEntry> children_;  // ascending chain_id (policy order)
};

}  // namespace blockchainer
