#include "aviary/observability/event_log.hpp"

#include <sstream>

#include "aviary/observability/events.hpp"

namespace aviary::observe {

namespace {
std::string hex(const crypto::Hash& h) {
  static constexpr char k[] = "0123456789abcdef";
  std::string s;
  for (std::uint8_t b : h) {
    s.push_back(k[b >> 4]);
    s.push_back(k[b & 0xF]);
  }
  return s;
}
}  // namespace

JsonEventLog::JsonEventLog(EventBus& bus) {
  bus.subscribe<BlockFinalized>([this](const BlockFinalized& e) {
    std::ostringstream os;
    os << "{\"type\":\"block_finalized\",\"height\":" << e.height
       << ",\"tx_count\":" << e.tx_count << ",\"block_id\":\"" << hex(e.block_id)
       << "\",\"state_root\":\"" << hex(e.state_root) << "\"}";
    emit(os.str());
  });
  bus.subscribe<BlockRejected>([this](const BlockRejected& e) {
    std::ostringstream os;
    os << "{\"type\":\"block_rejected\",\"attempted_height\":" << e.attempted_height
       << ",\"block_id\":\"" << hex(e.block_id) << "\"}";
    emit(os.str());
  });
  bus.subscribe<TxAdmitted>([this](const TxAdmitted& e) {
    std::ostringstream os;
    os << "{\"type\":\"tx_admitted\",\"txid\":\"" << hex(e.txid)
       << "\",\"mempool_size\":" << e.mempool_size << "}";
    emit(os.str());
  });
  bus.subscribe<TxRemoved>([this](const TxRemoved& e) {
    std::ostringstream os;
    os << "{\"type\":\"tx_removed\",\"txid\":\"" << hex(e.txid)
       << "\",\"mempool_size\":" << e.mempool_size << "}";
    emit(os.str());
  });
}

void JsonEventLog::emit(std::string json) {
  if (hook_) {
    hook_(json);
    return;
  }
  events_.push_back(std::move(json));
}

void JsonEventLog::set_hook(Hook hook) { hook_ = std::move(hook); }

std::string JsonEventLog::drain_json() {
  std::string out = "[";
  for (std::size_t i = 0; i < events_.size(); ++i) {
    if (i) out.push_back(',');
    out += events_[i];
  }
  out.push_back(']');
  events_.clear();
  return out;
}

}  // namespace aviary::observe
