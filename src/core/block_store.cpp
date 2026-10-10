#include "aviary/block_store.hpp"

#include <stdexcept>

namespace aviary {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Add a stored block's hash + tx indexes. Shared by put() and load().
void index(std::map<Hash, std::uint64_t>& hashes,
           std::map<Hash, BlockStore::TxLocation>& txs, std::uint64_t height,
           const Block& block) {
  hashes[block.header.hash()] = height;
  for (std::uint32_t i = 0; i < block.transactions.size(); ++i) {
    txs[block.transactions[i].txid()] = {height, i};
  }
}
}  // namespace

bool BlockStore::put(const Block& block, const consensus::CommitCertificate& commit) {
  const std::uint64_t h = block.header.height;
  if (has_height(h)) return false;                          // no overwrite
  if (!by_height_.empty() && h != tip_height() + 1) return false;  // must be contiguous
  by_height_[h] = StoredBlock{block, commit};
  index(hash_to_height_, tx_index_, h, block);
  return true;
}

const StoredBlock& BlockStore::by_height(std::uint64_t h) const {
  auto it = by_height_.find(h);
  if (it == by_height_.end()) throw std::out_of_range("block_store: no block at height");
  return it->second;
}

const StoredBlock& BlockStore::by_hash(const Hash& block_hash) const {
  auto it = hash_to_height_.find(block_hash);
  if (it == hash_to_height_.end()) throw std::out_of_range("block_store: unknown block hash");
  return by_height(it->second);
}

std::optional<BlockStore::TxLocation> BlockStore::find_tx(const Hash& txid) const {
  auto it = tx_index_.find(txid);
  if (it == tx_index_.end()) return std::nullopt;
  return it->second;
}

Bytes BlockStore::serialize() const {
  codec::CodecWriter w;
  w.write_u32(static_cast<std::uint32_t>(by_height_.size()));
  for (const auto& [height, sb] : by_height_) {  // map → ascending height, canonical
    w.write_u64(height);
    const Bytes blk = sb.block.encode();
    const Bytes cert = sb.commit.encode();
    w.write_bytes(view(blk));
    w.write_bytes(view(cert));
  }
  return w.take();
}

BlockStore BlockStore::load(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  BlockStore store;
  const std::uint32_t n = r.read_u32();
  for (std::uint32_t i = 0; i < n; ++i) {
    const std::uint64_t height = r.read_u64();
    const Bytes blk = r.read_bytes();
    const Bytes cert = r.read_bytes();
    StoredBlock sb;
    sb.block = Block::decode_all(view(blk));
    sb.commit = consensus::CommitCertificate::decode_all(view(cert));
    store.by_height_[height] = std::move(sb);
    index(store.hash_to_height_, store.tx_index_, height, store.by_height_[height].block);
  }
  r.assert_fully_consumed();
  return store;
}

}  // namespace aviary
