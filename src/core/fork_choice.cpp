#include "aviary/fork_choice.hpp"

#include <algorithm>
#include <vector>

namespace aviary {

ForkChoice::ForkChoice(ChainSpec spec,
                       std::function<std::unique_ptr<StateMachine>()> genesis_state,
                       std::uint64_t genesis_timestamp)
    : spec_(spec),
      genesis_state_(std::move(genesis_state)),
      genesis_timestamp_(genesis_timestamp) {
  canonical_ = std::make_unique<Ledger>(spec_, genesis_state_(), genesis_timestamp_);
  genesis_hash_ = canonical_->tip().hash();
  head_ = genesis_hash_;
}

std::uint64_t ForkChoice::weight_of(const Hash& head) const {
  if (head == genesis_hash_) return 0;
  auto it = nodes_.find(head);
  return it == nodes_.end() ? 0 : it->second.cumulative_weight;
}

std::unique_ptr<Ledger> ForkChoice::ledger_at(const Hash& block_hash) const {
  // Collect the branch genesis→block_hash by following parent links.
  std::vector<const Block*> branch;
  Hash cur = block_hash;
  while (cur != genesis_hash_) {
    auto it = nodes_.find(cur);
    if (it == nodes_.end()) return nullptr;  // dangling — should not happen
    branch.push_back(&it->second.block);
    cur = it->second.parent;
  }
  std::reverse(branch.begin(), branch.end());

  auto ledger = std::make_unique<Ledger>(spec_, genesis_state_(), genesis_timestamp_);
  for (const Block* b : branch) {
    if (!ledger->process_block(*b).ok) return nullptr;  // re-validate (should pass)
  }
  return ledger;
}

ForkChoice::AddResult ForkChoice::add_block(const Block& block, std::uint64_t weight) {
  if (weight == 0) weight = 1;  // a fork-choice weight is always positive
  const Hash id = block.header.hash();
  if (nodes_.count(id) != 0) return {false, false, "duplicate block"};

  // The parent must be genesis or a known block.
  const Hash parent = block.header.parent_hash;
  std::uint64_t parent_cum = 0;
  if (parent != genesis_hash_) {
    auto pit = nodes_.find(parent);
    if (pit == nodes_.end()) return {false, false, "unknown parent"};
    parent_cum = pit->second.cumulative_weight;
  }

  const std::uint64_t head_cum = weight_of(head_);
  const std::uint64_t new_cum = parent_cum + weight;

  if (parent == head_) {
    // Fast path: the block extends the current head. Validate + apply directly
    // (process_block is atomic — canonical_ is unchanged on failure). Extending
    // the heaviest branch always keeps it heaviest, so the head advances.
    if (BlockError be = canonical_->process_block(block); !be.ok) {
      return {false, false, be.error};
    }
    nodes_[id] = Node{block, parent, block.header.height, new_cum};
    head_ = id;
    return {true, false, ""};
  }

  // Side branch: build the parent's state and validate the block on it.
  auto branch_ledger = ledger_at(parent);
  if (!branch_ledger) return {false, false, "cannot reconstruct parent state"};
  if (BlockError be = branch_ledger->process_block(block); !be.ok) {
    return {false, false, be.error};
  }
  nodes_[id] = Node{block, parent, block.header.height, new_cum};

  // Fork choice: switch only if this branch is STRICTLY heavier (ties keep the
  // incumbent head → deterministic).
  if (new_cum > head_cum) {
    canonical_ = std::move(branch_ledger);  // already at the new head
    head_ = id;
    return {true, true, ""};  // reorg
  }
  return {true, false, ""};  // stored, but not canonical
}

}  // namespace aviary
