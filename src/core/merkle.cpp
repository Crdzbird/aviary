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
      // else: lone node carried up - no sibling recorded
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

std::optional<Hash> root_from_proof(const Hash& leaf, const Proof& proof, std::uint32_t leaf_count) {
  if (leaf_count == 0) return std::nullopt;
  if (proof.index >= leaf_count) return std::nullopt;

  Hash h = leaf;
  std::size_t idx = proof.index;
  std::size_t level_size = leaf_count;
  std::size_t si = 0;

  while (level_size > 1) {
    const bool has_sibling = (idx % 2 == 0) ? (idx + 1 < level_size) : true;
    if (has_sibling) {
      if (si >= proof.siblings.size()) return std::nullopt;
      const Hash& sib = proof.siblings[si++];
      h = (idx % 2 == 0) ? crypto::hash_node(h, sib) : crypto::hash_node(sib, h);
    }
    idx /= 2;
    level_size = (level_size + 1) / 2;
  }
  if (si != proof.siblings.size()) return std::nullopt;
  return h;
}

// -- MerkleIndex ----------------------------------------------------------------

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

Proof MerkleIndex::proof(std::uint32_t index) const {
  if (levels_.empty() || index >= levels_.front().size()) {
    throw std::out_of_range("MerkleIndex::proof: index out of range");
  }
  Proof proof;
  proof.index = index;
  std::size_t idx = index;
  for (std::size_t L = 0; L + 1 < levels_.size(); ++L, idx /= 2) {
    const auto& level = levels_[L];
    if (idx % 2 == 1) {
      proof.siblings.push_back(level[idx - 1]);
    } else if (idx + 1 < level.size()) {
      proof.siblings.push_back(level[idx + 1]);
    }
  }
  return proof;
}

Hash MerkleIndex::root_with_updates(const std::map<std::size_t, Hash>& changes) const {
  if (levels_.empty() || levels_.front().empty()) {
    return crypto::hash_tagged(crypto::Domain::kNode, {});
  }
  if (changes.empty()) return levels_.back().front();

  // Sparse overlay of recomputed nodes, keyed by (level, index). Every read
  // falls back to the cached level when a node was not recomputed, so only the
  // O(K-log N) nodes on the changed leaves' root-paths are ever hashed.
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

// -- AppendTree -----------------------------------------------------------------

struct AppendTree::Overlay {
  std::vector<std::size_t> sizes;
  std::map<std::pair<std::size_t, std::size_t>, Hash> nodes;
};

void AppendTree::reset(std::vector<Hash> leaves) {
  levels_.clear();
  if (leaves.empty()) return;
  levels_.push_back(std::move(leaves));
  while (levels_.back().size() > 1) levels_.push_back(next_level(levels_.back()));
}

const Hash& AppendTree::leaf(std::size_t index) const {
  if (index >= leaf_count()) throw std::out_of_range("AppendTree::leaf: index out of range");
  return levels_.front()[index];
}

AppendTree::Overlay AppendTree::overlay(const Batch& batch) const {
  const std::size_t base = leaf_count();
  Overlay ov;
  for (const auto& [index, hash] : batch.changes)
    if (index >= base) throw std::out_of_range("AppendTree: change names a leaf that does not exist");
  const std::size_t total = base + batch.appended.size();
  if (total == 0) return ov;
  ov.sizes.push_back(total);
  while (ov.sizes.back() > 1) ov.sizes.push_back((ov.sizes.back() + 1) / 2);

  std::set<std::size_t> dirty;
  for (const auto& [index, hash] : batch.changes) {
    ov.nodes[{0, index}] = hash;
    dirty.insert(index);
  }
  for (std::size_t i = 0; i < batch.appended.size(); ++i) {
    ov.nodes[{0, base + i}] = batch.appended[i];
    dirty.insert(base + i);
  }

  for (std::size_t level = 0; level + 1 < ov.sizes.size(); ++level) {
    std::set<std::size_t> parents;
    for (std::size_t i : dirty) parents.insert(i / 2);
    for (std::size_t p : parents) {
      auto at = [&](std::size_t idx) -> const Hash& {
        auto it = ov.nodes.find({level, idx});
        return it != ov.nodes.end() ? it->second : levels_[level][idx];
      };
      const std::size_t left = 2 * p, right = 2 * p + 1;
      ov.nodes[{level + 1, p}] = right < ov.sizes[level] ? crypto::hash_node(at(left), at(right)) : at(left);
    }
    dirty = std::move(parents);
  }
  return ov;
}

Hash AppendTree::root_with(const Batch& batch) const {
  const Overlay ov = overlay(batch);
  if (ov.sizes.empty()) return crypto::hash_tagged(crypto::Domain::kNode, {});
  auto it = ov.nodes.find({ov.sizes.size() - 1, 0});
  return it != ov.nodes.end() ? it->second : levels_.back().front();
}

Proof AppendTree::proof_with(const Batch& batch, std::uint32_t index) const {
  const Overlay ov = overlay(batch);
  if (ov.sizes.empty() || index >= ov.sizes.front())
    throw std::out_of_range("AppendTree::proof_with: index out of range");
  Proof proof;
  proof.index = index;
  std::size_t idx = index;
  for (std::size_t level = 0; level + 1 < ov.sizes.size(); ++level, idx /= 2) {
    auto at = [&](std::size_t i) -> const Hash& {
      auto it = ov.nodes.find({level, i});
      return it != ov.nodes.end() ? it->second : levels_[level][i];
    };
    if (idx % 2 == 1) {
      proof.siblings.push_back(at(idx - 1));
    } else if (idx + 1 < ov.sizes[level]) {
      proof.siblings.push_back(at(idx + 1));
    }
  }
  return proof;
}

void AppendTree::apply(const Batch& batch) {
  Overlay ov = overlay(batch);
  if (ov.sizes.empty()) return;
  levels_.resize(ov.sizes.size());
  for (std::size_t level = 0; level < ov.sizes.size(); ++level) levels_[level].resize(ov.sizes[level]);
  for (const auto& [where, hash] : ov.nodes) levels_[where.first][where.second] = hash;
}

}  // namespace aviary::merkle
