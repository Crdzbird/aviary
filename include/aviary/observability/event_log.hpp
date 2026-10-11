#pragma once
#include <functional>
#include <string>
#include <vector>

#include "aviary/observability/event_bus.hpp"

/// The JSON event surface every node kind shares: subscribes to the typed bus,
/// renders each event as one JSON object, and either buffers it for a drain
/// or hands it to a hook synchronously. The field names are the streaming
/// contract the bindings decode (block_finalized: height, tx_count, block_id,
/// state_root; block_rejected: attempted_height, block_id; tx_admitted and
/// tx_removed: txid, mempool_size).
namespace aviary::observe {

class JsonEventLog {
 public:
  using Hook = std::function<void(const std::string& json)>;

  /// Subscribes to [bus] for the lifetime of this log; the log must outlive
  /// no bus and must not move.
  explicit JsonEventLog(EventBus& bus);
  JsonEventLog(const JsonEventLog&) = delete;
  JsonEventLog& operator=(const JsonEventLog&) = delete;

  /// While a hook is set, events go to it instead of the buffer.
  void set_hook(Hook hook);
  /// The buffered events as a JSON array, emptying the buffer.
  std::string drain_json();
  /// Drops the buffered events.
  void clear() { events_.clear(); }

 private:
  void emit(std::string json);
  std::vector<std::string> events_;
  Hook hook_;
};

}  // namespace aviary::observe
