#include <optional>
#include "aviary/subchain_relay.hpp"

#include <algorithm>

namespace aviary::node {

std::optional<Anchor> pending_anchor(const SubchainRegistry& parent, std::uint64_t child_chain_id,
                                     const BlockStore& child_archive, std::uint16_t limit) {
  const BlockHeader* tip = parent.tip_of(child_chain_id);
  if (tip == nullptr) return std::nullopt;
  std::uint64_t cap = parent.policy().max_headers_per_anchor;
  if (limit != 0 && (cap == 0 || limit < cap)) cap = limit;
  Anchor a;
  a.child_chain_id = child_chain_id;
  for (std::uint64_t h = tip->height + 1; h <= child_archive.tip_height(); ++h) {
    if (cap != 0 && a.headers.size() >= cap) break;
    if (!child_archive.has_height(h)) break;  // a gap: nothing consecutive beyond it
    const StoredBlock& sb = child_archive.by_height(h);
    a.headers.push_back(CertifiedHeader{sb.block.header, sb.commit.encode()});
  }
  if (a.headers.empty()) return std::nullopt;
  return a;
}

std::vector<Anchor> pending_anchors(const SubchainRegistry& parent,
                                    const std::vector<std::pair<std::uint64_t, const BlockStore*>>& children) {
  std::vector<Anchor> out;
  for (const auto& [id, archive] : children) {
    if (archive == nullptr) continue;
    if (auto a = pending_anchor(parent, id, *archive)) out.push_back(std::move(*a));
  }
  std::sort(out.begin(), out.end(), [](const Anchor& x, const Anchor& y) { return x.child_chain_id < y.child_chain_id; });
  out.erase(std::unique(out.begin(), out.end(), [](const Anchor& x, const Anchor& y) { return x.child_chain_id == y.child_chain_id; }), out.end());
  const std::uint16_t max = parent.policy().max_anchors_per_block;
  if (max != 0 && out.size() > max) out.resize(max);
  return out;
}

}  // namespace aviary::node
