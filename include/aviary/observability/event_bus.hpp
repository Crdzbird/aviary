#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

/// A typed publish/subscribe event bus - the streaming API consumers subscribe
/// to for live chain state (ARCHITECTURE section 12). Strongly typed: subscribers get
/// exactly the event type they ask for. Dispatch is synchronous and in
/// subscription order (deterministic).
///
/// Observability is a side-channel: publishing an event must never feed back
/// into consensus state or hashing. Event payloads carry deterministic data
/// (heights, hashes), never wall-clock or randomness from the core.
namespace aviary::observe {

class EventBus {
 public:
  using SubscriptionId = std::uint64_t;

  /// Subscribe to events of type E. Returns an id for [unsubscribe].
  template <class E>
  SubscriptionId subscribe(std::function<void(const E&)> handler) {
    const SubscriptionId id = ++last_id_;
    auto& list = handlers_[std::type_index(typeid(E))];
    list.push_back({id, [h = std::move(handler)](const void* p) {
                      h(*static_cast<const E*>(p));
                    }});
    return id;
  }

  /// Publish an event of type E to all subscribers, in subscription order.
  template <class E>
  void publish(const E& event) const {
    auto it = handlers_.find(std::type_index(typeid(E)));
    if (it == handlers_.end()) return;
    for (const auto& entry : it->second) entry.fn(&event);
  }

  void unsubscribe(SubscriptionId id) {
    for (auto& [type, list] : handlers_) {
      for (auto i = list.begin(); i != list.end(); ++i) {
        if (i->id == id) {
          list.erase(i);
          return;
        }
      }
    }
  }

 private:
  struct Entry {
    SubscriptionId id;
    std::function<void(const void*)> fn;
  };
  std::unordered_map<std::type_index, std::vector<Entry>> handlers_;
  SubscriptionId last_id_ = 0;
};

}  // namespace aviary::observe
