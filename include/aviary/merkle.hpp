#pragma once

#include <stdexcept>
#include <cstdint>
#include <map>
#include <optional>
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

/// The root that [leaf] at proof.index implies under a tree of [leaf_count]
/// leaves, or nullopt when the proof does not have exactly the siblings that
/// shape needs. verify_proof(root, ...) is root_from_proof(...) == root; this
/// form serves a commitment that wraps the Merkle root in an outer hash.
std::optional<Hash> root_from_proof(const Hash& leaf, const Proof& proof, std::uint32_t leaf_count);

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

/// A cached Merkle tree over a leaf array that only grows at the end and whose
/// existing leaves may be rewritten: the shape of an append-only registry.
/// Root and proofs are byte-identical to compute_root / make_proof over the
/// same leaves (same odd-node carry-up rule).
///
/// A Batch describes a set of rewrites and appends against the current tree.
/// root_with and proof_with answer for the tree AFTER the batch without
/// touching the cache, in O((K + A) log N) hashes for K rewrites and A appends;
/// apply folds the batch in at the same cost. That lets a block be validated
/// on a working copy that shares this tree, and committed or dropped.
class AppendTree {
 public:
  struct Batch {
    std::map<std::size_t, Hash> changes;  ///< existing position -> new leaf hash
    std::vector<Hash> appended;           ///< new leaves, placed after the last, in order
  };

  /// Rebuild the cache from a leaf array. O(N).
  void reset(std::vector<Hash> leaves);

  std::size_t leaf_count() const { return levels_.empty() ? 0 : levels_.front().size(); }
  /// The leaf hash at [index]. Throws std::out_of_range if absent.
  const Hash& leaf(std::size_t index) const;

  /// Current root. Empty tree -> the fixed domain-separated empty root.
  Hash root() const { return root_with(Batch{}); }

  /// Root after [batch]. Throws std::out_of_range if a change names a position
  /// that is not an existing leaf. Pure.
  Hash root_with(const Batch& batch) const;

  /// Inclusion proof for leaf [index] of the tree after [batch]. Throws
  /// std::out_of_range if that tree has no such leaf. Pure.
  Proof proof_with(const Batch& batch, std::uint32_t index) const;

  /// Fold [batch] into the cache.
  void apply(const Batch& batch);

 private:
  struct Overlay;
  Overlay overlay(const Batch& batch) const;

  std::vector<std::vector<Hash>> levels_;
};

}  // namespace aviary::merkle
