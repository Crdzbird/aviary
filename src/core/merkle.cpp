#include "blockchainer/merkle.hpp"

#include <stdexcept>

namespace blockchainer::merkle {

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

}  // namespace blockchainer::merkle
