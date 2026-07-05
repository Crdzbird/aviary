#pragma once

#include <cstdint>
#include <map>
#include <optional>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/types.hpp"

/// Persistent, indexed block archive (ARCHITECTURE §13). Stores finalized blocks
/// with their commit certificates, contiguous by height, and indexes them by
/// block hash and by transaction id. Serializes to a byte blob so a node can
/// persist its chain and reload it on restart — the block half of persistence
/// (the state half is `Ledger::snapshot`).
namespace blockchainer {

/// A finalized block plus the certificate that finalized it.
struct StoredBlock {
  Block block;
  consensus::CommitCertificate commit;
};

class BlockStore {
 public:
  /// Where a transaction lives: which block height, and its index within it.
  struct TxLocation {
    std::uint64_t height = 0;
    std::uint32_t index = 0;
  };

  /// Append a finalized block + its commit certificate. Heights must be
  /// contiguous (each one after the previous), and a height may not be stored
  /// twice — returns false otherwise.
  bool put(const Block& block, const consensus::CommitCertificate& commit);

  bool empty() const { return by_height_.empty(); }
  std::size_t size() const { return by_height_.size(); }
  /// Highest stored height (0 if empty).
  std::uint64_t tip_height() const { return by_height_.empty() ? 0 : by_height_.rbegin()->first; }

  bool has_height(std::uint64_t h) const { return by_height_.count(h) != 0; }
  bool has_hash(const Hash& block_hash) const { return hash_to_height_.count(block_hash) != 0; }

  /// Retrieve by height / by block hash. Throws std::out_of_range if absent.
  const StoredBlock& by_height(std::uint64_t h) const;
  const StoredBlock& by_hash(const Hash& block_hash) const;

  /// Locate a transaction by id, or nullopt if unknown.
  std::optional<TxLocation> find_tx(const Hash& txid) const;

  /// Serialize the whole archive (blocks + certs) to bytes; the indexes are
  /// rebuilt on load.
  Bytes serialize() const;
  static BlockStore load(codec::ByteView bytes);

 private:
  std::map<std::uint64_t, StoredBlock> by_height_;
  std::map<Hash, std::uint64_t> hash_to_height_;
  std::map<Hash, TxLocation> tx_index_;
};

}  // namespace blockchainer
