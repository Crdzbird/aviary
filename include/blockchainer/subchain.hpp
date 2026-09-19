#pragma once

#include <optional>
#include <string>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/crypto.hpp"
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

/// A child registered at the parent's genesis: its identity (genesis hash), the
/// trusted starting header (its genesis header) and its FIXED validator set.
struct ChildRegistration {
  std::uint64_t chain_id = 0;
  Hash genesis_hash{};
  BlockHeader genesis_header;
  std::vector<consensus::Validator> validators;

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
  /// header at height 0 with matching chain_id. Empty string = valid.
  std::string validate(std::uint64_t own_chain_id) const;

  void commit_to(GenesisSpec& g) const;
  static std::optional<SubchainPolicy> from_genesis(const GenesisSpec& g);
  bool operator==(const SubchainPolicy&) const = default;
};

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

  /// `subchain_root`: hash_tagged(kSubchainRegistry, count ‖ per child
  /// ascending: chain_id ‖ genesis_hash ‖ tip.hash() ‖ tip.height ‖ tip.state_root).
  Hash root() const;

  /// The mutable part (child tips) for snapshots; the registrations come from
  /// the policy. `restore_tips` throws on a blob that does not match the policy.
  Bytes serialize_tips() const;
  void restore_tips(crypto::ByteView bytes);

 private:
  SubchainPolicy policy_;
  std::vector<ChildEntry> children_;  // ascending chain_id (policy order)
};

}  // namespace blockchainer
