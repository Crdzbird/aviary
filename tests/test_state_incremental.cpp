// Differential + property tests for the copy-on-write / incremental-Merkle
// AccountStateMachine (P1) and its MerkleIndex helper.
//
// The optimisation is consensus-critical: the accelerated state root MUST equal
// the naive `compute_root` over the sorted account leaves for EVERY reachable
// state, or nodes fork. So we cross-check the fast path against a naive
// reference over long randomised op sequences that deliberately drive every
// branch — the O(1) clean read, the O(K·log N) incremental update, the insert
// rebuild, and the large-overlay seal — and confirm clones are independent.

#include <cstdint>
#include <map>
#include <random>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/merkle.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {

Hash addr_from(std::uint64_t n) {
  // Spread bytes so addresses sort in a non-trivial order (not just by a
  // trailing counter), exercising real leaf reordering.
  Hash h{};
  for (int i = 0; i < 32; ++i) h[i] = static_cast<std::uint8_t>((n * 2654435761u) >> ((i % 4) * 8));
  h[0] = static_cast<std::uint8_t>(n);
  h[31] = static_cast<std::uint8_t>(n >> 8);
  return h;
}

Bytes leaf_preimage(const Hash& addr, codec::u128 balance, std::uint64_t nonce) {
  codec::CodecWriter w;
  w.write_fixed_bytes(codec::ByteView(addr.data(), addr.size()), 32);
  w.write_u128(balance);
  w.write_u64(nonce);
  return w.take();
}

// Naive reference root: exactly the pre-optimisation definition.
Hash naive_root(const std::map<Hash, AccountStateMachine::Account>& accts) {
  std::vector<Hash> leaves;
  leaves.reserve(accts.size());
  for (const auto& [addr, acc] : accts) {
    leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf,
                                         leaf_preimage(addr, acc.balance, acc.nonce)));
  }
  return merkle::compute_root(leaves);
}

// ── MerkleIndex: root_with_updates == compute_root of the mutated leaves ──────
void test_merkle_index() {
  std::mt19937_64 rng(0xC0FFEE);
  for (int trial = 0; trial < 200; ++trial) {
    const std::size_t n = 1 + (rng() % 40);
    std::vector<Hash> leaves(n);
    for (auto& l : leaves) {
      for (auto& b : l) b = static_cast<std::uint8_t>(rng());
    }
    merkle::MerkleIndex idx;
    idx.reset(leaves);
    CHECK(idx.root() == merkle::compute_root(leaves));

    // Random subset of positions gets new values.
    std::map<std::size_t, Hash> changes;
    std::vector<Hash> mutated = leaves;
    for (std::size_t i = 0; i < n; ++i) {
      if (rng() % 3 == 0) {
        Hash nh{};
        for (auto& b : nh) b = static_cast<std::uint8_t>(rng());
        changes[i] = nh;
        mutated[i] = nh;
      }
    }
    CHECK(idx.root_with_updates(changes) == merkle::compute_root(mutated));
    // Pure: the cache is untouched by root_with_updates.
    CHECK(idx.root() == merkle::compute_root(leaves));
  }
  // No-op update returns the base root; empty tree is the empty root.
  merkle::MerkleIndex empty;
  empty.reset({});
  CHECK(empty.root() == merkle::compute_root({}));
}

// ── AccountStateMachine: fast root tracks the naive root through everything ───
void test_account_state_differential() {
  std::mt19937_64 rng(0xBADF00D);
  AccountStateMachine sm;
  std::map<Hash, AccountStateMachine::Account> ref;  // ground truth
  std::uint64_t universe = 1;                        // grows as accounts appear

  CHECK(sm.state_root() == naive_root(ref));  // empty

  for (int step = 0; step < 4000; ++step) {
    const bool insert = ref.empty() || (rng() % 4 == 0);
    Hash target;
    if (insert) {
      target = addr_from(universe++);
    } else {
      // Pick an existing account to drive the updates-only incremental path.
      auto it = ref.begin();
      std::advance(it, rng() % ref.size());
      target = it->first;
    }
    const codec::u128 amount = 1 + (rng() % 1000);

    sm.credit_genesis(target, amount);
    ref[target].balance += amount;

    // Check the root frequently (every step for the first stretch, then
    // sampled) so both the pre-seal overlay path and post-seal reads are hit.
    if (step < 300 || step % 7 == 0) {
      CHECK_MSG(sm.state_root() == naive_root(ref), "root matches naive after op");
    }
    // Balance read must reflect base+overlay.
    if (rng() % 50 == 0) {
      CHECK(sm.account(target).balance == ref[target].balance);
    }
  }
  CHECK_MSG(sm.state_root() == naive_root(ref), "final root matches naive");

  // A proof against the final (large) state must verify under that root.
  auto some = ref.begin();
  std::advance(some, ref.size() / 2);
  StateProof p = sm.prove_account(some->first);
  CHECK(p.present);
  const Hash root = sm.state_root();
  Hash leaf = crypto::hash_tagged(crypto::Domain::kLeaf,
                                  leaf_preimage(some->first, some->second.balance,
                                                some->second.nonce));
  merkle::Proof mp{p.proof.index, p.proof.siblings};
  CHECK_MSG(merkle::verify_proof(root, leaf, mp, p.leaf_count),
            "inclusion proof verifies against fast root");
}

// ── clone() is a copy-on-write snapshot: mutating one must not touch the other ─
void test_clone_independence() {
  AccountStateMachine a;
  for (std::uint64_t i = 0; i < 500; ++i) a.credit_genesis(addr_from(i), 100);
  const Hash root_a = a.state_root();

  auto b = a.clone();
  auto* bp = dynamic_cast<AccountStateMachine*>(b.get());
  // Mutate the clone heavily; the original must be frozen.
  for (std::uint64_t i = 0; i < 500; ++i) bp->credit_genesis(addr_from(i), 7);
  bp->credit_genesis(addr_from(9999), 1);

  CHECK_MSG(a.state_root() == root_a, "original root unchanged after clone mutated");
  CHECK(a.account(addr_from(0)).balance == 100);
  CHECK(bp->account(addr_from(0)).balance == 107);

  // The clone's root equals a freshly-built machine with the clone's balances.
  std::map<Hash, AccountStateMachine::Account> ref;
  for (std::uint64_t i = 0; i < 500; ++i) ref[addr_from(i)].balance = 107;
  ref[addr_from(9999)].balance = 1;
  CHECK_MSG(bp->state_root() == naive_root(ref), "clone root matches its balances");
}

}  // namespace

int main() {
  std::printf("== MerkleIndex incremental ==\n");
  test_merkle_index();
  std::printf("== AccountStateMachine differential ==\n");
  test_account_state_differential();
  std::printf("== clone COW independence ==\n");
  test_clone_independence();
  return bctest::summary();
}
