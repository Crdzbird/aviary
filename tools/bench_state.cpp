// Micro-benchmark for the P1 incremental state commitment.
//
// The pre-optimisation cost of committing a block was O(total accounts): the
// state root rehashed every leaf. This shows the new cost of committing a
// block that moves value between EXISTING accounts is ~flat as the account set
// grows, by timing the accelerated `state_root()` after a single-account
// mutation against the naive full recompute over the same set.
//
// Not a correctness test (test_state_incremental covers that) — just evidence
// the DoS lever is gone. Build: `cmake --build build --target bench_state`.
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/merkle.hpp"

using namespace aviary;
using clk = std::chrono::steady_clock;

static Hash addr_from(std::uint64_t n) {
  Hash h{};
  for (int i = 0; i < 32; ++i) h[i] = static_cast<std::uint8_t>((n * 2654435761u) >> ((i % 4) * 8));
  h[0] = static_cast<std::uint8_t>(n);
  h[31] = static_cast<std::uint8_t>(n >> 8);
  return h;
}

static Bytes preimage(const Hash& a, codec::u128 bal, std::uint64_t nonce) {
  codec::CodecWriter w;
  w.write_fixed_bytes(codec::ByteView(a.data(), a.size()), 32);
  w.write_u128(bal);
  w.write_u64(nonce);
  return w.take();
}

int main() {
  std::printf("%10s %16s %16s %10s\n", "accounts", "naive_root(us)", "incremental(us)", "speedup");
  for (std::uint64_t n : {1000ull, 4000ull, 16000ull, 64000ull, 256000ull}) {
    AccountStateMachine sm;
    std::map<Hash, AccountStateMachine::Account> ref;
    for (std::uint64_t i = 0; i < n; ++i) {
      sm.credit_genesis(addr_from(i), 1000);
      ref[addr_from(i)].balance = 1000;
    }
    sm.state_root();  // force the one-time seal so the tree is cached

    constexpr int kReps = 50;
    // Naive: rebuild the whole tree from all leaves every time (old behaviour).
    auto t0 = clk::now();
    for (int r = 0; r < kReps; ++r) {
      std::vector<Hash> leaves;
      leaves.reserve(ref.size());
      for (const auto& [a, acc] : ref) {
        leaves.push_back(crypto::hash_tagged(crypto::Domain::kLeaf,
                                             preimage(a, acc.balance + r, acc.nonce)));
      }
      volatile auto root = merkle::compute_root(leaves);
      (void)root;
    }
    auto naive_us = std::chrono::duration_cast<std::chrono::microseconds>(clk::now() - t0).count() /
                    double(kReps);

    // Incremental: mutate one existing account, recompute the root (new path).
    auto t1 = clk::now();
    for (int r = 0; r < kReps; ++r) {
      sm.credit_genesis(addr_from(r % 32), 1);  // update-in-place, no new account
      volatile auto root = sm.state_root();
      (void)root;
    }
    auto inc_us = std::chrono::duration_cast<std::chrono::microseconds>(clk::now() - t1).count() /
                  double(kReps);

    std::printf("%10llu %16.1f %16.2f %9.0fx\n", (unsigned long long)n, naive_us, inc_us,
                naive_us / (inc_us > 0 ? inc_us : 1));
  }
  return 0;
}
