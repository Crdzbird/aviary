#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

/// Metrics primitives + a registry with a Prometheus text exporter
/// (ARCHITECTURE §12). Counters/gauges are lock-free; the histogram is
/// mutex-guarded. Export order is deterministic (sorted by name).
namespace aviary::observe {

class Counter {
 public:
  void inc(std::uint64_t n = 1) { v_.fetch_add(n, std::memory_order_relaxed); }
  std::uint64_t value() const { return v_.load(std::memory_order_relaxed); }

 private:
  std::atomic<std::uint64_t> v_{0};
};

class Gauge {
 public:
  void set(std::int64_t v) { v_.store(v, std::memory_order_relaxed); }
  void inc(std::int64_t n = 1) { v_.fetch_add(n, std::memory_order_relaxed); }
  void dec(std::int64_t n = 1) { v_.fetch_sub(n, std::memory_order_relaxed); }
  std::int64_t value() const { return v_.load(std::memory_order_relaxed); }

 private:
  std::atomic<std::int64_t> v_{0};
};

class Histogram {
 public:
  /// [upper_bounds] must be ascending (the implicit +Inf bucket is added).
  explicit Histogram(std::vector<double> upper_bounds);

  void observe(double value);

  struct Snapshot {
    std::vector<double> bounds;             ///< finite upper bounds
    std::vector<std::uint64_t> cumulative;  ///< count <= bounds[i]
    std::uint64_t count = 0;
    double sum = 0.0;
  };
  Snapshot snapshot() const;

 private:
  mutable std::mutex mu_;
  std::vector<double> bounds_;
  std::vector<std::uint64_t> counts_;  // cumulative per bound
  std::uint64_t count_ = 0;
  double sum_ = 0.0;
};

class MetricsRegistry {
 public:
  /// Get-or-create. Repeated calls with the same name return the same metric.
  Counter& counter(const std::string& name, const std::string& help = "");
  Gauge& gauge(const std::string& name, const std::string& help = "");
  Histogram& histogram(const std::string& name, std::vector<double> buckets,
                       const std::string& help = "");

  /// Prometheus text exposition format. Deterministic (names sorted).
  std::string prometheus_text() const;

 private:
  template <class T>
  struct Entry {
    std::unique_ptr<T> metric;
    std::string help;
  };
  std::map<std::string, Entry<Counter>> counters_;
  std::map<std::string, Entry<Gauge>> gauges_;
  std::map<std::string, Entry<Histogram>> histograms_;
};

}  // namespace aviary::observe
