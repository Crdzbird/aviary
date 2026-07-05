#include "blockchainer/ledger.hpp"

#include <stdexcept>

namespace blockchainer {

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
  tip_.version = 1;
  tip_.chain_id = spec_.chain_id;
  tip_.height = 0;
  tip_.parent_hash = crypto::zero_hash();
  tip_.timestamp = genesis_timestamp;
  tip_.tx_root = compute_tx_root({});
  tip_.state_root = state_->state_root();
}

BlockError Ledger::check_header(const BlockHeader& h) const {
  if (h.chain_id != spec_.chain_id) return BlockError::failure("chain_id mismatch");
  if (h.height != tip_.height + 1) return BlockError::failure("non-sequential height");
  if (h.parent_hash != tip_.hash()) return BlockError::failure("parent hash mismatch");
  if (h.timestamp < tip_.timestamp) return BlockError::failure("non-monotonic timestamp");
  return BlockError::success();
}

Block Ledger::build_block(std::vector<Transaction> txs, std::uint64_t timestamp) {
  if (std::string e = block_limit_error(spec_, txs); !e.empty()) {
    throw std::runtime_error("build_block: " + e);
  }
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
    if (spec_.produce_receipts) receipts.push_back(Receipt{1, working->tx_fee(tx)});  // pre-apply
    working->apply(tx);
  }

  Block block;
  block.header.version = spec_.produce_receipts ? 2 : 1;
  block.header.chain_id = spec_.chain_id;
  block.header.height = tip_.height + 1;
  block.header.parent_hash = tip_.hash();
  block.header.timestamp = timestamp;
  block.header.tx_root = compute_tx_root(txs);
  block.header.state_root = working->state_root();
  if (spec_.produce_receipts) block.header.receipt_root = compute_receipt_root(receipts);
  block.transactions = std::move(txs);
  return block;
}

BlockError Ledger::process_block(const Block& block) {
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

  // 4. Per-tx: signature verify → state validate → apply, on a working copy so
  // the commit is all-or-nothing.
  auto working = state_->clone();
  const bool with_receipts = block.header.version >= 2;
  std::vector<Receipt> receipts;
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
    const ValidationResult vr = working->validate(tx);
    if (!vr.ok) return BlockError::failure("invalid tx: " + vr.error);
    if (with_receipts) receipts.push_back(Receipt{1, working->tx_fee(tx)});  // pre-apply
    working->apply(tx);
  }

  // 5. Recompute and compare the post-state root.
  if (working->state_root() != block.header.state_root) {
    return BlockError::failure("state_root mismatch");
  }

  // 5a. Recompute and compare the execution receipt root (version-2 headers).
  if (with_receipts && compute_receipt_root(receipts) != block.header.receipt_root) {
    return BlockError::failure("receipt_root mismatch");
  }

  // 6. (consensus verification — M2)
  // 7. Commit atomically.
  state_ = std::move(working);
  last_receipts_ = std::move(receipts);
  tip_ = block.header;
  return BlockError::success();
}

Bytes Ledger::snapshot() const {
  codec::CodecWriter w;
  const Bytes tip_bytes = tip_.encode();
  const Bytes state_bytes = state_->serialize();
  w.write_bytes(codec::ByteView(tip_bytes.data(), tip_bytes.size()));
  w.write_bytes(codec::ByteView(state_bytes.data(), state_bytes.size()));
  return w.take();
}

std::unique_ptr<Ledger> Ledger::restore(ChainSpec spec, codec::ByteView snapshot) {
  codec::CodecReader r(snapshot);
  const Bytes tip_bytes = r.read_bytes();
  const Bytes state_bytes = r.read_bytes();
  r.assert_fully_consumed();

  BlockHeader tip = BlockHeader::decode_all(codec::ByteView(tip_bytes.data(), tip_bytes.size()));
  auto state = deserialize_state(spec.state_model, codec::ByteView(state_bytes.data(), state_bytes.size()));

  // The public constructor seeds a genesis tip; overwrite it with the restored
  // tip so the chain resumes at the snapshotted height.
  auto ledger = std::make_unique<Ledger>(spec, std::move(state), tip.timestamp);
  ledger->tip_ = tip;
  return ledger;
}

}  // namespace blockchainer
