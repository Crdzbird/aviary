#include <map>
#include "aviary/merkle.hpp"

#include <set>
#include <stdexcept>
#include <utility>

namespace aviary::merkle {

namespace {
// Build the next level up, carrying a lone trailing node up unchanged.
std::vector<Hash> next_level(const std::vector<Hash>& level) {
  std::vector<Hash> next;
  next.reserve((level.size() + 1) / 2);
  std::size_t i = 0;
  for (; i + 1 < level.size(); i += 2) {
    next.push_back(crypto::hash_node(level[i], level[i + 1]));
  }
  if (i < level.size()) next.push_back(level[i]);  // lone node carried up
  return next;
}
}  // namespace

Hash compute_root(const std::vector<Hash>& leaves) {
  if (leaves.empty()) {
    // Fixed, domain-separated empty root.
    return crypto::hash_tagged(crypto::Domain::kNode, {});
  }
  std::vector<Hash> level = leaves;
  while (level.size() > 1) {
    level = next_level(level);
  }
  return level[0];
}

Proof make_proof(const std::vector<Hash>& leaves, std::uint32_t index) {
  if (index >= leaves.size()) {
    throw std::out_of_range("merkle::make_proof: index out of range");
  }
  Proof proof;
  proof.index = index;

  std::vector<Hash> level = leaves;
  std::size_t idx = index;
  while (level.size() > 1) {
    if (idx % 2 == 0) {
      if (idx + 1 < level.size()) {
        proof.siblings.push_back(level[idx + 1]);  // sibling on the right
      }
      // else: lone node carried up — no sibling recorded
    } else {
      proof.siblings.push_back(level[idx - 1]);  // sibling on the left
    }
    level = next_level(level);
    idx /= 2;
  }
  return proof;
}

bool verify_proof(const Hash& root, const Hash& leaf, const Proof& proof,
                  std::uint32_t leaf_count) {
  if (leaf_count == 0) return false;
  if (proof.index >= leaf_count) return false;

  Hash h = leaf;
  std::size_t idx = proof.index;
  std::size_t level_size = leaf_count;
  std::size_t si = 0;

  while (level_size > 1) {
    const bool has_sibling = (idx % 2 == 0) ? (idx + 1 < level_size) : true;
    if (has_sibling) {
      if (si >= proof.siblings.size()) return false;
      const Hash& sib = proof.siblings[si++];
      h = (idx % 2 == 0) ? crypto::hash_node(h, sib) : crypto::hash_node(sib, h);
    }
    idx /= 2;
    level_size = (level_size + 1) / 2;
  }
  return si == proof.siblings.size() && h == root;
}

// ── MerkleIndex ────────────────────────────────────────────────────────────────

void MerkleIndex::reset(std::vector<Hash> leaves) {
  levels_.clear();
  if (leaves.empty()) return;  // root() returns the fixed empty root
  levels_.push_back(std::move(leaves));
  while (levels_.back().size() > 1) {
    levels_.push_back(next_level(levels_.back()));
  }
}

Hash MerkleIndex::root() const {
  if (levels_.empty() || levels_.front().empty()) {
    return crypto::hash_tagged(crypto::Domain::kNode, {});
  }
  return levels_.back().front();
}

Hash MerkleIndex::root_with_updates(const std::map<std::size_t, Hash>& changes) const {
  if (levels_.empty() || levels_.front().empty()) {
    return crypto::hash_tagged(crypto::Domain::kNode, {});
  }
  if (changes.empty()) return levels_.back().front();

  // Sparse overlay of recomputed nodes, keyed by (level, index). Every read
  // falls back to the cached level when a node was not recomputed, so only the
  // O(K·log N) nodes on the changed leaves' root-paths are ever hashed.
  std::map<std::pair<std::size_t, std::size_t>, Hash> ov;
  std::set<std::size_t> dirty;
  for (const auto& [i, h] : changes) {
    if (i >= levels_.front().size()) {
      throw std::out_of_range("MerkleIndex::root_with_updates: index out of range");
    }
    ov[{0, i}] = h;
    dirty.insert(i);
  }

  for (std::size_t L = 0; L + 1 < levels_.size(); ++L) {
    const auto& level = levels_[L];
    std::set<std::size_t> parents;
    for (std::size_t i : dirty) parents.insert(i / 2);
    for (std::size_t p : parents) {
      auto at = [&](std::size_t idx) -> const Hash& {
        auto it = ov.find({L, idx});
        return it != ov.end() ? it->second : level[idx];
      };
      const std::size_t l = 2 * p, r = 2 * p + 1;
      ov[{L + 1, p}] =
          (r < level.size()) ? crypto::hash_node(at(l), at(r)) : at(l);  // lone carry-up
    }
    dirty = std::move(parents);
  }

  auto it = ov.find({levels_.size() - 1, 0});
  return it != ov.end() ? it->second : levels_.back().front();
}

}  // namespace aviary::merkle
