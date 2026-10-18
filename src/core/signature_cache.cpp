#include "aviary/signature_cache.hpp"

#include <cstring>

namespace aviary {

SignatureCache::SignatureCache(std::size_t capacity) : capacity_(capacity == 0 ? 1 : capacity) {
  set_.reserve(capacity_);
}

std::size_t SignatureCache::LeafHasher::operator()(const crypto::Hash& h) const noexcept {
  std::size_t v = 0;
  std::memcpy(&v, h.data(), sizeof v);
  return v;
}

bool SignatureCache::verify(const Transaction& tx, const crypto::Hash& leaf) {
  if (set_.count(leaf) != 0) return true;
  if (!verify_transaction_signature(tx)) return false;
  if (order_.size() == capacity_) {
    set_.erase(order_.front());
    order_.pop_front();
  }
  set_.insert(leaf);
  order_.push_back(leaf);
  return true;
}

}
