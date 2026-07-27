#include "blockchainer/mempool.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "blockchainer/credential_gate.hpp"
#include "blockchainer/observability/event_bus.hpp"
#include "blockchainer/observability/events.hpp"

namespace blockchainer {

namespace {
// The sender bucket used for per-account accounting: BLAKE3(author). Absent for
// authorless/multi-author (e.g. UTXO) txs, which are not per-account capped.
std::optional<Hash> sender_bucket(const Transaction& tx) {
  if (tx.author.empty()) return std::nullopt;
  return crypto::address_of(codec::ByteView(tx.author.data(), tx.author.size()));
}
}  // namespace

AddResult Mempool::add(const Transaction& tx, std::uint64_t chain_id, std::uint32_t epoch,
                       crypto::ByteView challenge, crypto::ByteView presentation,
                       std::uint64_t now) {
  // Membership admission first (a permissioned chain): prove holder of an issuer-
  // signed credential clearing the policy, unlinkably, before spending any effort
  // on the tx itself. An ungated pool skips straight to the normal add.
  if (gate_ != nullptr && !gate_->admit(epoch, "submit", challenge, presentation, now)) {
    return AddResult::kNotAdmitted;
  }
  return add(tx, chain_id);
}

AddResult Mempool::add(const Transaction& tx, std::uint64_t chain_id) {
  if (tx.chain_id != chain_id) return AddResult::kWrongChain;

  const Bytes enc = tx.encode();
  if (enc.size() > cfg_.max_tx_bytes) return AddResult::kTooLarge;

  const Hash id = tx.txid();
  if (pool_.count(id) != 0) return AddResult::kDuplicate;

  if (!verify_transaction_signature(tx)) return AddResult::kInvalidSignature;

  // Per-account cap: one sender cannot flood the pool and starve others.
  const std::optional<Hash> bucket = sender_bucket(tx);
  if (cfg_.max_per_account != 0 && bucket) {
    auto it = per_account_.find(*bucket);
    if (it != per_account_.end() && it->second >= cfg_.max_per_account) {
      return AddResult::kAccountLimit;
    }
  }

  // Capacity: when full, run a fee-priority eviction — the cheapest resident is
  // dropped only if the newcomer outbids it. So a full pool keeps the highest
  // paying txs; a spammer must outbid to displace anyone.
  if (pool_.size() >= cfg_.max_txs) {
    auto victim = pool_.end();
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
      // Lowest declared fee wins the eviction lottery; tie broken by the larger
      // txid so the choice is deterministic.
      if (victim == pool_.end() ||
          it->second.declared_fee() < victim->second.declared_fee() ||
          (it->second.declared_fee() == victim->second.declared_fee() &&
           victim->first < it->first)) {
        victim = it;
      }
    }
    if (victim == pool_.end() || tx.declared_fee() <= victim->second.declared_fee()) {
      return AddResult::kFull;  // newcomer does not outbid the cheapest resident
    }
    if (const std::optional<Hash> vb = sender_bucket(victim->second)) {
      auto pit = per_account_.find(*vb);
      if (pit != per_account_.end() && --pit->second == 0) per_account_.erase(pit);
    }
    const Hash victim_id = victim->first;
    pool_.erase(victim);
    if (bus_) bus_->publish(observe::TxRemoved{victim_id, pool_.size()});
  }

  pool_.emplace(id, tx);
  if (bucket) ++per_account_[*bucket];
  if (bus_) bus_->publish(observe::TxAdmitted{id, pool_.size()});
  return AddResult::kAdded;
}

std::vector<Transaction> Mempool::select(const StateMachine& state,
                                         std::size_t max) const {
  auto working = state.clone();
  std::vector<Transaction> out;
  std::set<Hash> used;
  std::size_t block_bytes = 0;

  // Candidate order: highest declared fee first (txid ascending as a
  // deterministic tiebreak). Higher-paying txs are offered to the block first.
  std::vector<std::pair<Hash, const Transaction*>> ordered;
  ordered.reserve(pool_.size());
  for (const auto& [id, tx] : pool_) ordered.emplace_back(id, &tx);
  std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
    const codec::u128 fa = a.second->declared_fee(), fb = b.second->declared_fee();
    if (fa != fb) return fa > fb;   // higher fee first
    return a.first < b.first;       // then lowest txid
  });

  // Fixed-point: repeatedly apply any pending tx that validates against the
  // evolving state. A tx that depends on another (account nonce n needs n-1
  // applied; a UTXO spending a not-yet-created output) becomes ready in a later
  // pass. The fee-ordered scan makes selection deterministic and fee-priority.
  //
  // Selection stops at whichever block limit binds first — tx count [max] or the
  // configured total-bytes budget — so a block's size is bounded regardless of
  // how the pool is shaped.
  bool progress = true;
  while (progress && out.size() < max) {
    progress = false;
    for (const auto& [id, txp] : ordered) {
      if (out.size() >= max) break;
      if (used.count(id) != 0) continue;
      const Transaction& tx = *txp;
      const std::size_t tx_bytes = tx.encode().size();
      if (cfg_.max_block_bytes != 0 && block_bytes + tx_bytes > cfg_.max_block_bytes) {
        // Skip this tx (too big for the remaining budget) but keep scanning —
        // a smaller ready tx may still fit.
        continue;
      }
      if (working->validate(tx).ok) {
        working->apply(tx);
        out.push_back(tx);
        used.insert(id);
        block_bytes += tx_bytes;
        progress = true;
      }
    }
  }
  return out;
}

void Mempool::remove(const std::vector<Hash>& txids) {
  for (const auto& id : txids) {
    auto it = pool_.find(id);
    if (it == pool_.end()) continue;
    if (const std::optional<Hash> bucket = sender_bucket(it->second)) {
      auto pit = per_account_.find(*bucket);
      if (pit != per_account_.end() && --pit->second == 0) per_account_.erase(pit);
    }
    pool_.erase(it);
    if (bus_) bus_->publish(observe::TxRemoved{id, pool_.size()});
  }
}

}  // namespace blockchainer
