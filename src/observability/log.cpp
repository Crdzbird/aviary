#include "blockchainer/observability/log.hpp"

#include <sstream>

namespace blockchainer::observe {

const char* to_string(LogLevel level) {
  switch (level) {
    case LogLevel::kTrace: return "TRACE";
    case LogLevel::kDebug: return "DEBUG";
    case LogLevel::kInfo: return "INFO";
    case LogLevel::kWarn: return "WARN";
    case LogLevel::kError: return "ERROR";
  }
  return "?";
}

std::string format(const LogRecord& record) {
  std::ostringstream os;
  os << to_string(record.level) << ' ' << record.message;
  for (const auto& f : record.fields) os << ' ' << f.key << '=' << f.value;
  return os.str();
}

void Logger::log(LogLevel level, std::string message, std::vector<LogField> fields) {
  if (sink_ == nullptr || level < min_level_) return;
  sink_->write(LogRecord{level, std::move(message), std::move(fields)});
}

}  // namespace blockchainer::observe
