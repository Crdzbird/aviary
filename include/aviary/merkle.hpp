#pragma once

#include <stdexcept>
#include <cstdint>
#include <map>
#include <vector>

#include "aviary/crypto.hpp"

/// Binary Merkle tree over leaf hashes (ARCHITECTURE section 4, docs/CODEC_SPEC.md).
///
/// Odd-node rule: a lone node is **carried up unchanged** (never duplicated),
/// which avoids the CVE-2012-2459 duplicate-node malleability class. Node hashes
/// are domain-separated (`hash_node` = H(0x01 || left || right)); leaves are
/// expected to already be domain-separated by the caller (`hash_tagged(kLeaf,...)`).
namespace aviary::merkle {

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

/// A cached, incrementally-updatable binary Merkle tree over an ordered leaf
/// array. Its root is **byte-identical** to `compute_root(leaves)` - same odd-
/// node carry-up rule - so it is a drop-in acceleration for state commitments
/// that mostly mutate existing leaves rather than resizing.
///
/// `reset` builds all levels once (O(N)). `root_with_updates` answers "what
/// would the root be if these leaf positions changed?" in O(K-log N) WITHOUT
/// mutating the cache - untouched siblings are read straight from the cached
/// levels. This lets a working state snapshot commit a block's handful of
/// account changes without rehashing the whole tree.
class MerkleIndex {
 public:
  /// Rebuild the cache from an ordered leaf-hash array. O(N).
  void reset(std::vector<Hash> leaves);

  std::size_t leaf_count() const { return levels_.empty() ? 0 : levels_.front().size(); }

  /// Current cached root. Empty tree -> the fixed domain-separated empty root.
  Hash root() const;

  /// Root that WOULD result if leaf[i] became changes[i], for each i. Positions
  /// must be existing leaves (no resize). Pure - does not touch the cache.
  Hash root_with_updates(const std::map<std::size_t, Hash>& changes) const;

  /// Inclusion proof for leaf [index], read from the cached levels in
  /// O(log N). Byte-identical to `make_proof(leaves, index)`. Throws
  /// std::out_of_range if the index is not a leaf.
  Proof proof(std::uint32_t index) const;

 private:
  // levels_[0] == leaves; each level up is next_level() of the one below;
  // levels_.back() == {root}. Empty tree -> levels_ is empty.
  std::vector<std::vector<Hash>> levels_;
};

}  // namespace aviary::merkle
