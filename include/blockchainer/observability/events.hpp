#pragma once

#include <cstdint>

#include "blockchainer/crypto.hpp"

/// Chain event types published on the EventBus (ARCHITECTURE §12). Payloads are
/// deterministic — heights and hashes the core already computed.
namespace blockchainer::observe {

using crypto::Hash;

/// A block was finalized and applied.
struct BlockFinalized {
  std::uint64_t height = 0;
  Hash block_id{};
  Hash state_root{};
  std::uint64_t tx_count = 0;
};

/// A block was rejected (failed consensus or state validation).
struct BlockRejected {
  std::uint64_t attempted_height = 0;
  Hash block_id{};
};

/// A transaction was admitted to the mempool.
struct TxAdmitted {
  Hash txid{};
  std::uint64_t mempool_size = 0;
};

/// A transaction left the mempool (included in a block or evicted).
struct TxRemoved {
  Hash txid{};
  std::uint64_t mempool_size = 0;
};

}  // namespace blockchainer::observe
