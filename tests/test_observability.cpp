#include <string>
#include <vector>

#include "aviary/observability/event_bus.hpp"
#include "aviary/observability/events.hpp"
#include "aviary/observability/log.hpp"
#include "aviary/observability/metrics.hpp"
#include "check.hpp"

using namespace aviary::observe;

namespace {
bool contains(const std::string& hay, const std::string& needle) {
  return hay.find(needle) != std::string::npos;
}
}  // namespace

int main() {
  std::printf("== event bus: typed dispatch ==\n");
  {
    EventBus bus;
    int finalized_count = 0;
    std::uint64_t last_height = 0;
    bus.subscribe<BlockFinalized>([&](const BlockFinalized& e) {
      ++finalized_count;
      last_height = e.height;
    });
    int tx_count = 0;
    bus.subscribe<TxAdmitted>([&](const TxAdmitted&) { ++tx_count; });

    bus.publish(BlockFinalized{7, {}, {}, 3});
    bus.publish(TxAdmitted{{}, 1});
    bus.publish(BlockFinalized{8, {}, {}, 0});

    CHECK(finalized_count == 2);  // only BlockFinalized handlers fired
    CHECK(last_height == 8);
    CHECK(tx_count == 1);  // type isolation
  }

  std::printf("== event bus: ordering + unsubscribe ==\n");
  {
    EventBus bus;
    std::vector<int> order;
    bus.subscribe<BlockFinalized>([&](const BlockFinalized&) { order.push_back(1); });
    auto id = bus.subscribe<BlockFinalized>([&](const BlockFinalized&) { order.push_back(2); });
    bus.subscribe<BlockFinalized>([&](const BlockFinalized&) { order.push_back(3); });
    bus.publish(BlockFinalized{});
    CHECK((order == std::vector<int>{1, 2, 3}));  // subscription order
    order.clear();
    bus.unsubscribe(id);
    bus.publish(BlockFinalized{});
    CHECK((order == std::vector<int>{1, 3}));
  }

  std::printf("== metrics: counter / gauge ==\n");
  {
    MetricsRegistry reg;
    auto& c = reg.counter("blocks_total", "blocks finalized");
    c.inc();
    c.inc(2);
    CHECK(c.value() == 3);
    CHECK(&reg.counter("blocks_total") == &c);  // get-or-create returns same

    auto& g = reg.gauge("chain_height");
    g.set(10);
    g.inc();
    g.dec(3);
    CHECK(g.value() == 8);
  }

  std::printf("== metrics: histogram ==\n");
  {
    MetricsRegistry reg;
    auto& h = reg.histogram("validate_seconds", {0.1, 0.5, 1.0});
    h.observe(0.05);
    h.observe(0.2);
    h.observe(0.7);
    h.observe(5.0);
    auto s = h.snapshot();
    CHECK(s.count == 4);
    CHECK(s.cumulative[0] == 1);  // <= 0.1
    CHECK(s.cumulative[1] == 2);  // <= 0.5
    CHECK(s.cumulative[2] == 3);  // <= 1.0
  }

  std::printf("== metrics: prometheus exposition ==\n");
  {
    MetricsRegistry reg;
    reg.counter("blocks_total", "blocks finalized").inc(3);
    reg.gauge("chain_height").set(8);
    const std::string text = reg.prometheus_text();
    CHECK(contains(text, "# TYPE blocks_total counter"));
    CHECK(contains(text, "blocks_total 3"));
    CHECK(contains(text, "# TYPE chain_height gauge"));
    CHECK(contains(text, "chain_height 8"));
  }

  std::printf("== logging: levels, fields, sink ==\n");
  {
    BufferSink sink;
    Logger log(&sink, LogLevel::kInfo);
    log.debug("ignored below threshold");
    log.info("block finalized", {{"height", "7"}, {"txs", "3"}});
    log.error("bad block");
    CHECK(sink.records.size() == 2);  // debug filtered out
    CHECK(format(sink.records[0]) == "INFO block finalized height=7 txs=3");
    CHECK(sink.records[1].level == LogLevel::kError);

    // Null sink is a no-op.
    Logger noop(nullptr);
    noop.error("nowhere");  // must not crash
  }

  return bctest::summary();
}
