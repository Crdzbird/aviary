#pragma once

#include <cstdint>
#include <vector>

#include "blockchainer/crypto.hpp"

/// Binary Merkle tree over leaf hashes (ARCHITECTURE §4, docs/CODEC_SPEC.md).
///
/// Odd-node rule: a lone node is **carried up unchanged** (never duplicated),
/// which avoids the CVE-2012-2459 duplicate-node malleability class. Node hashes
/// are domain-separated (`hash_node` = H(0x01 ‖ left ‖ right)); leaves are
/// expected to already be domain-separated by the caller (`hash_tagged(kLeaf,…)`).
namespace blockchainer::merkle {

using crypto::Hash;

/// Root over [leaves]. An empty tree has a fixed domain-separated empty root;
/// a single leaf is its own root.
Hash compute_root(const std::vector<Hash>& leaves);

/// An inclusion proof: the sibling hashes from leaf to root, bottom-up.
struct Proof {
  std::uint32_t index = 0;
  std::vector<Hash> siblings;
};

/// Builds an inclusion proof for [index]. Throws std::out_of_range if the index
/// is not within [leaves].
Proof make_proof(const std::vector<Hash>& leaves, std::uint32_t index);

/// Verifies that [leaf] at proof.index is included under [root], given the total
/// [leaf_count] (needed to reconstruct the tree shape under the carry-up rule).
bool verify_proof(const Hash& root, const Hash& leaf, const Proof& proof,
                  std::uint32_t leaf_count);

}  // namespace blockchainer::merkle
