#include "aviary/ledger.hpp"

#include <optional>
#include <stdexcept>

namespace aviary {

namespace {
// Total encoded size of a block's transactions — the quantity ChainSpec bounds.
std::uint64_t total_tx_bytes(const std::vector<Transaction>& txs) {
  std::uint64_t n = 0;
  for (const auto& tx : txs) n += tx.encode().size();
  return n;
}

// Whether [txs] fits the chain's block ceilings. Empty message = fits.
std::string block_limit_error(const ChainSpec& spec, const std::vector<Transaction>& txs) {
  if (spec.max_block_txs != 0 && txs.size() > spec.max_block_txs) {
    return "block exceeds max tx count";
  }
  if (spec.max_block_bytes != 0 && total_tx_bytes(txs) > spec.max_block_bytes) {
    return "block exceeds max byte size";
  }
  return {};
}
}  // namespace

Ledger::Ledger(ChainSpec spec, std::unique_ptr<StateMachine> genesis_state,
               std::uint64_t genesis_timestamp)
    : spec_(spec), state_(std::move(genesis_state)) {
  if (!spec_.subchain_policy.empty()) {
    // A parent chain: an invalid policy is a boot failure, never a silent
    // "anchors nothing" (std::invalid_argument propagates).
    registry_ = SubchainRegistry::from_policy(
        spec_.chain_id, SubchainPolicy::decode_all(
                            crypto::ByteView(spec_.subchain_policy.data(), spec_.subchain_policy.size())));
  }
  tip_.version = registry_ ? 3 : 1;
  tip_.chain_id = spec_.chain_id;
  tip_.height = 0;
  tip_.parent_hash = crypto::zero_hash();
  tip_.timestamp = genesis_timestamp;
  tip_.tx_root = compute_tx_root({});
  tip_.state_root = state_->state_root();
  if (registry_) tip_.subchain_root = registry_->root();  // anchor_root = zero (no anchors)
}

BlockError Ledger::check_header(const BlockHeader& h) const {
  if (h.chain_id != spec_.chain_id) return BlockError::failure("chain_id mismatch");
  // Version fence (docs/design/SUBCHAINS.md I2): anchor-carrying headers only
  // on a parent chain, and a parent chain only accepts anchor-carrying headers
  // (a lower version would skip the subchain_root commitment).
  if (h.version >= 3 && !registry_) return BlockError::failure("version-3 header on a chain with no subchain policy");
  if (h.version < 3 && registry_) return BlockError::failure("parent chain requires version-3 headers");
  if (h.height != tip_.height + 1) return BlockError::failure("non-sequential height");
  if (h.parent_hash != tip_.hash()) return BlockError::failure("parent hash mismatch");
  if (h.timestamp < tip_.timestamp) return BlockError::failure("non-monotonic timestamp");
  return BlockError::success();
}

Block Ledger::build_block(std::vector<Transaction> txs, std::uint64_t timestamp,
                          std::vector<Anchor> anchors) {
  if (std::string e = block_limit_error(spec_, txs); !e.empty()) {
    throw std::runtime_error("build_block: " + e);
  }
  if (!anchors.empty() && !registry_) throw std::runtime_error("build_block: anchors on a non-parent chain");
  std::optional<SubchainRegistry> reg = registry_;  // working copy
  if (reg) {
    if (std::string e = reg->apply_anchors(anchors); !e.empty()) throw std::runtime_error("build_block: invalid anchor: " + e);
  }
  const bool with_receipts = spec_.produce_receipts || registry_.has_value();  // v3 implies v2's receipts
  auto working = state_->clone();
  std::vector<Receipt> receipts;
  for (const auto& tx : txs) {
    if (tx.chain_id != spec_.chain_id) {
      throw std::runtime_error("build_block: tx chain_id mismatch");
    }
    if (!spec_.accepts(tx.algo())) {
      throw std::runtime_error("build_block: signature scheme not accepted by chain");
    }
    if (!verify_transaction_signature(tx)) {
      throw std::runtime_error("build_block: bad signature");
    }
    const ValidationResult vr = working->validate(tx);
    if (!vr.ok) throw std::runtime_error("build_block: invalid tx: " + vr.error);
    if (with_receipts) receipts.push_back(Receipt{1, working->tx_fee(tx)});  // pre-apply
    working->apply(tx);
  }

  Block block;
  block.header.version = registry_ ? 3 : (spec_.produce_receipts ? 2 : 1);
  block.header.chain_id = spec_.chain_id;
  block.header.height = tip_.height + 1;
  block.header.parent_hash = tip_.hash();
  block.header.timestamp = timestamp;
  block.header.tx_root = compute_tx_root(txs);
  block.header.state_root = working->state_root();
  if (with_receipts) block.header.receipt_root = compute_receipt_root(receipts);
  if (reg) {
    block.header.anchor_root = compute_anchor_root(anchors);
    block.header.subchain_root = reg->root();
    block.anchors = std::move(anchors);
  }
  block.transactions = std::move(txs);
  return block;
}

BlockError Ledger::run_block(const Block& block, Worked& out) const {
  // 2. Header checks (1. decode happened to construct the Block).
  if (BlockError he = check_header(block.header); !he.ok) return he;

  // 2a. Block-size ceilings: reject an oversized block before doing per-tx work,
  // so a peer cannot force O(huge) verification with one message.
  if (std::string e = block_limit_error(spec_, block.transactions); !e.empty()) {
    return BlockError::failure(e);
  }

  // 3. Recompute and compare the transaction root.
  if (compute_tx_root(block.transactions) != block.header.tx_root) {
    return BlockError::failure("tx_root mismatch");
  }

  // 3b/3c. Subchain anchors (docs/design/SUBCHAINS.md §6): structural root +
  // order first, then the per-header light-client verification — all on a
  // working copy of the registry, BEFORE any transaction work.
  if (!block.anchors.empty() && !registry_) return BlockError::failure("anchors on a chain with no subchain policy");
  out.registry = registry_;
  if (out.registry) {
    if (compute_anchor_root(block.anchors) != block.header.anchor_root) {
      return BlockError::failure("anchor_root mismatch");
    }
    if (std::string e = out.registry->apply_anchors(block.anchors); !e.empty()) {
      return BlockError::failure("invalid anchor: " + e);
    }
  }

  // 4. Per-tx: signature verify → state validate → apply, on a working copy so
  // the commit is all-or-nothing.
  out.state = state_->clone();
  const bool with_receipts = block.header.version >= 2;
  out.receipts.clear();
  for (const auto& tx : block.transactions) {
    if (tx.chain_id != spec_.chain_id) {
      return BlockError::failure("tx chain_id mismatch");
    }
    if (!spec_.accepts(tx.algo())) {
      return BlockError::failure("signature scheme not accepted by chain");
    }
    if (!verify_transaction_signature(tx)) {
      return BlockError::failure("invalid tx signature");
    }
    const ValidationResult vr = out.state->validate(tx);
    if (!vr.ok) return BlockError::failure("invalid tx: " + vr.error);
    if (with_receipts) out.receipts.push_back(Receipt{1, out.state->tx_fee(tx)});  // pre-apply
    out.state->apply(tx);
  }

  // 5. Recompute and compare the post-state root.
  if (out.state->state_root() != block.header.state_root) {
    return BlockError::failure("state_root mismatch");
  }

  // 5a. Recompute and compare the execution receipt root (version-2 headers).
  if (with_receipts && compute_receipt_root(out.receipts) != block.header.receipt_root) {
    return BlockError::failure("receipt_root mismatch");
  }

  // 5b. The post-anchor registry commitment.
  if (out.registry && out.registry->root() != block.header.subchain_root) {
    return BlockError::failure("subchain_root mismatch");
  }
  return BlockError::success();
}

BlockError Ledger::validate_block(const Block& block) const {
  Worked scratch;
  return run_block(block, scratch);  // scratch is dropped: no state change
}

BlockError Ledger::process_block(const Block& block) {
  Worked w;
  if (BlockError e = run_block(block, w); !e.ok) return e;
  // 6. (consensus verification — the engine's job)
  // 7. Commit atomically.
  state_ = std::move(w.state);
  last_receipts_ = std::move(w.receipts);
  if (w.registry) registry_ = std::move(w.registry);
  tip_ = block.header;
  return BlockError::success();
}

Bytes Ledger::snapshot() const {
  codec::CodecWriter w;
  const Bytes tip_bytes = tip_.encode();
  const Bytes state_bytes = state_->serialize();
  w.write_bytes(codec::ByteView(tip_bytes.data(), tip_bytes.size()));
  w.write_bytes(codec::ByteView(state_bytes.data(), state_bytes.size()));
  if (registry_) {  // parent chains append their child tips; others are byte-identical to before
    const Bytes reg_bytes = registry_->serialize_tips();
    w.write_bytes(codec::ByteView(reg_bytes.data(), reg_bytes.size()));
  }
  return w.take();
}

std::unique_ptr<Ledger> Ledger::restore(ChainSpec spec, codec::ByteView snapshot) {
  codec::CodecReader r(snapshot);
  const Bytes tip_bytes = r.read_bytes();
  const Bytes state_bytes = r.read_bytes();
  Bytes reg_bytes;
  if (!spec.subchain_policy.empty()) reg_bytes = r.read_bytes();
  r.assert_fully_consumed();

  BlockHeader tip = BlockHeader::decode_all(codec::ByteView(tip_bytes.data(), tip_bytes.size()));
  auto state = deserialize_state(spec.state_model, codec::ByteView(state_bytes.data(), state_bytes.size()));

  // The public constructor seeds a genesis tip; overwrite it with the restored
  // tip so the chain resumes at the snapshotted height.
  auto ledger = std::make_unique<Ledger>(spec, std::move(state), tip.timestamp);
  ledger->tip_ = tip;
  if (ledger->registry_) {
    ledger->registry_->restore_tips(codec::ByteView(reg_bytes.data(), reg_bytes.size()));
    if (ledger->registry_->root() != tip.subchain_root) throw std::invalid_argument("snapshot: subchain_root mismatch");
  }
  return ledger;
}

}  // namespace aviary
