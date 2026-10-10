#include <memory>
#include "aviary/observability/metrics.hpp"

#include <sstream>

namespace aviary::observe {

Histogram::Histogram(std::vector<double> upper_bounds)
    : bounds_(std::move(upper_bounds)), counts_(bounds_.size(), 0) {}

void Histogram::observe(double value) {
  std::lock_guard<std::mutex> lock(mu_);
  for (std::size_t i = 0; i < bounds_.size(); ++i) {
    if (value <= bounds_[i]) ++counts_[i];
  }
  ++count_;
  sum_ += value;
}

Histogram::Snapshot Histogram::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  Snapshot s;
  s.bounds = bounds_;
  s.cumulative = counts_;
  s.count = count_;
  s.sum = sum_;
  return s;
}

Counter& MetricsRegistry::counter(const std::string& name, const std::string& help) {
  auto it = counters_.find(name);
  if (it == counters_.end()) {
    it = counters_.emplace(name, Entry<Counter>{std::make_unique<Counter>(), help}).first;
  }
  return *it->second.metric;
}

Gauge& MetricsRegistry::gauge(const std::string& name, const std::string& help) {
  auto it = gauges_.find(name);
  if (it == gauges_.end()) {
    it = gauges_.emplace(name, Entry<Gauge>{std::make_unique<Gauge>(), help}).first;
  }
  return *it->second.metric;
}

Histogram& MetricsRegistry::histogram(const std::string& name,
                                      std::vector<double> buckets,
                                      const std::string& help) {
  auto it = histograms_.find(name);
  if (it == histograms_.end()) {
    it = histograms_
             .emplace(name, Entry<Histogram>{
                                std::make_unique<Histogram>(std::move(buckets)), help})
             .first;
  }
  return *it->second.metric;
}

namespace {
std::string fmt_double(double d) {
  std::ostringstream os;
  os << d;
  return os.str();
}
}  // namespace

std::string MetricsRegistry::prometheus_text() const {
  std::ostringstream os;

  for (const auto& [name, e] : counters_) {
    if (!e.help.empty()) os << "# HELP " << name << ' ' << e.help << '\n';
    os << "# TYPE " << name << " counter\n";
    os << name << ' ' << e.metric->value() << '\n';
  }
  for (const auto& [name, e] : gauges_) {
    if (!e.help.empty()) os << "# HELP " << name << ' ' << e.help << '\n';
    os << "# TYPE " << name << " gauge\n";
    os << name << ' ' << e.metric->value() << '\n';
  }
  for (const auto& [name, e] : histograms_) {
    if (!e.help.empty()) os << "# HELP " << name << ' ' << e.help << '\n';
    os << "# TYPE " << name << " histogram\n";
    const Histogram::Snapshot s = e.metric->snapshot();
    for (std::size_t i = 0; i < s.bounds.size(); ++i) {
      os << name << "_bucket{le=\"" << fmt_double(s.bounds[i]) << "\"} "
         << s.cumulative[i] << '\n';
    }
    os << name << "_bucket{le=\"+Inf\"} " << s.count << '\n';
    os << name << "_sum " << fmt_double(s.sum) << '\n';
    os << name << "_count " << s.count << '\n';
  }
  return os.str();
}

}  // namespace aviary::observe
