#include "blockchainer/gossip.hpp"

namespace blockchainer::p2p {

namespace {
Hash message_id(const Bytes& message) {
  static const std::uint8_t kEmpty = 0;
  const std::uint8_t* ptr = message.empty() ? &kEmpty : message.data();
  return crypto::blake3_256(ptr, message.size());
}
}  // namespace

bool GossipOverlay::seen(const Bytes& message) const {
  return seen_.count(message_id(message)) != 0;
}

std::vector<GossipOverlay::Relay> GossipOverlay::flood(const Bytes& message,
                                                       std::optional<std::uint32_t> except) {
  std::vector<Relay> out;
  out.reserve(peers_.size());
  for (std::uint32_t peer : peers_) {
    if (except && peer == *except) continue;
    out.push_back({peer, message});
  }
  return out;
}

std::vector<GossipOverlay::Relay> GossipOverlay::originate(const Bytes& message) {
  seen_.insert(message_id(message));  // never re-accept our own message
  return flood(message, std::nullopt);
}

std::vector<GossipOverlay::Relay> GossipOverlay::on_receive(std::uint32_t from,
                                                            const Bytes& message) {
  if (!seen_.insert(message_id(message)).second) {
    return {};  // already seen → drop (stops loops and floods)
  }
  return flood(message, from);
}

}  // namespace blockchainer::p2p
