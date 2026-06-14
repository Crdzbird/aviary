#pragma once

#include <string>
#include <vector>

/// Structured, leveled logging with a pluggable sink (ARCHITECTURE §12). The
/// core emits no wall-clock; a sink may add timestamps. Records carry structured
/// key/value fields (height, round, peer, tx hash) rather than interpolated text.
namespace blockchainer::observe {

enum class LogLevel { kTrace = 0, kDebug, kInfo, kWarn, kError };

const char* to_string(LogLevel level);

struct LogField {
  std::string key;
  std::string value;
};

struct LogRecord {
  LogLevel level = LogLevel::kInfo;
  std::string message;
  std::vector<LogField> fields;
};

/// Stable single-line rendering: `LEVEL message key=value key=value`.
std::string format(const LogRecord& record);

class LogSink {
 public:
  virtual ~LogSink() = default;
  virtual void write(const LogRecord& record) = 0;
};

/// Captures records in memory — for tests and inspection.
class BufferSink : public LogSink {
 public:
  void write(const LogRecord& record) override { records.push_back(record); }
  std::vector<LogRecord> records;
};

class Logger {
 public:
  /// [sink] may be null (logging becomes a no-op). [min_level] filters records.
  explicit Logger(LogSink* sink = nullptr, LogLevel min_level = LogLevel::kInfo)
      : sink_(sink), min_level_(min_level) {}

  void log(LogLevel level, std::string message, std::vector<LogField> fields = {});

  void trace(std::string m, std::vector<LogField> f = {}) { log(LogLevel::kTrace, std::move(m), std::move(f)); }
  void debug(std::string m, std::vector<LogField> f = {}) { log(LogLevel::kDebug, std::move(m), std::move(f)); }
  void info(std::string m, std::vector<LogField> f = {}) { log(LogLevel::kInfo, std::move(m), std::move(f)); }
  void warn(std::string m, std::vector<LogField> f = {}) { log(LogLevel::kWarn, std::move(m), std::move(f)); }
  void error(std::string m, std::vector<LogField> f = {}) { log(LogLevel::kError, std::move(m), std::move(f)); }

 private:
  LogSink* sink_;
  LogLevel min_level_;
};

}  // namespace blockchainer::observe
