#pragma once

#include <cstddef>
#include <deque>
#include <unordered_set>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

namespace aviary {

/// Transactions whose signature already verified, keyed by the transaction's
/// tx root leaf (`tx_leaf_hash`), which commits to its whole encoding: author,
/// scheme, chain id, body and signature. Verification is a pure function of
/// that encoding, so a hit stands in for verifying again and can never change
/// a result; a cache miss only costs the verification it saves. Bounded, the
/// oldest entry evicted first. Single threaded, like the node.
class SignatureCache {
 public:
  static constexpr std::size_t kDefaultCapacity = 8192;

  explicit SignatureCache(std::size_t capacity = kDefaultCapacity);

  /// verify_transaction_signature(tx), skipped when [leaf] is cached; a pass
  /// is remembered. [leaf] must be tx_leaf_hash(tx).
  bool verify(const Transaction& tx, const crypto::Hash& leaf);

  bool contains(const crypto::Hash& leaf) const { return set_.count(leaf) != 0; }
  std::size_t size() const { return order_.size(); }
  std::size_t capacity() const { return capacity_; }

 private:
  struct LeafHasher {
    std::size_t operator()(const crypto::Hash& h) const noexcept;
  };
  std::size_t capacity_;
  std::unordered_set<crypto::Hash, LeafHasher> set_;
  std::deque<crypto::Hash> order_;
};

}
