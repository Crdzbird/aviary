#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "aviary/block_store.hpp"
#include "aviary/subchain.hpp"

/// The in-process relay (docs/design/SUBCHAINS.md section 9 Q6): what a parent
/// proposer should anchor next, computed from a child's finalized archive and
/// the parent's current registry. Pure functions - deterministic, no I/O - so a
/// proposer's choice is reproducible and every validator re-verifies it anyway.
namespace aviary::node {

/// The next anchor for [child_chain_id]: the consecutive run of the child's
/// finalized headers after the parent's recorded tip, bounded by the parent
/// policy's max_headers_per_anchor (and [limit] if non-zero). nullopt when the
/// child is not registered, its archive holds nothing newer, or the archive has
/// a gap right after the recorded tip (nothing consecutive to anchor).
std::optional<Anchor> pending_anchor(const SubchainRegistry& parent, std::uint64_t child_chain_id,
                                     const BlockStore& child_archive, std::uint16_t limit = 0);

/// All pending anchors for the given (child id, archive) pairs, in canonical
/// order and bounded by max_anchors_per_block - ready for propose_anchored.
std::vector<Anchor> pending_anchors(const SubchainRegistry& parent,
                                    const std::vector<std::pair<std::uint64_t, const BlockStore*>>& children);

}  // namespace aviary::node
