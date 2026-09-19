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

}  // namespace blockchainer
