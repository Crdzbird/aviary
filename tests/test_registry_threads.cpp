// The component registry and the foreign engine registry are process global
// and reached from any thread an embedding runtime uses (Dart isolates, Go
// goroutines, JVM threads). Registration, discovery, lookup and assembly must
// be safe to interleave. Run under ThreadSanitizer (AVIARY_SANITIZE_THREAD).
#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/genesis.hpp"
#include "aviary/node/demo_node.hpp"
#include "aviary/foreign_engine.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

int main() {
  init();
  constexpr int kThreads = 8;
  constexpr int kRounds = 40;
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([t, &failures] {
      for (int r = 0; r < kRounds; ++r) {
        const std::string id = "fuzz-engine-" + std::to_string(t) + "-" + std::to_string(r);
        node::register_foreign_engine(
            id, [](const Block&) { return Bytes{1}; },
            [](const Block&, codec::ByteView j) { return j.size() == 1; }, false);
        const auto ids = node::ComponentRegistry::engine_ids();
        bool found = false;
        for (const auto& e : ids) found = found || e == id;
        if (!found || node::ComponentRegistry::find_engine(id) == nullptr) failures.fetch_add(1);
        if (node::ComponentRegistry::find_engine("bft") == nullptr) failures.fetch_add(1);
        if (node::ComponentRegistry::find_state("account") == nullptr) failures.fetch_add(1);
        node::DemoNode n(static_cast<std::uint64_t>(1000 + t * kRounds + r));
        auto a = n.new_account();
        const Hash addr = crypto::address_of(view(a.public_key));
        n.credit(addr, 10);
        if (n.produce_block(1) != 1) failures.fetch_add(1);
      }
    });
  }
  for (auto& th : threads) th.join();
  CHECK(failures.load() == 0);
  CHECK(node::ComponentRegistry::engine_ids().size() >= static_cast<std::size_t>(kThreads * kRounds + 4));
  return bctest::summary();
}
