#include "blockchainer/gossip.hpp"

#include "blockchainer/credential_gate.hpp"

namespace blockchainer::p2p {

bool GossipOverlay::admit_peer(std::uint32_t peer, std::uint32_t epoch, crypto::ByteView challenge,
                               crypto::ByteView presentation, std::uint64_t now) {
  // Membership admission before a peer joins the mesh: prove holder of an issuer-
  // signed credential clearing the policy, unlinkably, under the "gossip" scope.
  // An ungated overlay admits everyone (permissionless).
  if (gate_ != nullptr && !gate_->admit(epoch, "gossip", challenge, presentation, now)) {
    return false;
  }
  add_peer(peer);
  return true;
}

namespace {
Hash message_id(const Bytes& message) {
  static const std::uint8_t kEmpty = 0;
  const std::uint8_t* ptr = message.empty() ? &kEmpty : message.data();
  return crypto::blake3_256(ptr, message.size());
}
}  // namespace

bool GossipOverlay::mark_seen(const Hash& id) {
  if (!seen_.insert(id).second) return false;  // already within the window
  seen_order_.push_back(id);
  if (seen_.size() > cfg_.seen_capacity) {
    // Evict the oldest id so the window stays bounded. The window must exceed the
    // in-flight message count, else a still-propagating message could re-enter.
    const Hash oldest = seen_order_.front();
    seen_order_.pop_front();
    seen_.erase(oldest);
  }
  return true;
}

bool GossipOverlay::seen(const Bytes& message) const {
  return seen_.count(message_id(message)) != 0;
}

std::vector<GossipOverlay::Relay> GossipOverlay::flood(const Bytes& message,
                                                       std::optional<std::uint32_t> except) {
  std::vector<Relay> out;
  out.reserve(peers_.size());
  for (std::uint32_t peer : peers_) {
    if (except && peer == *except) continue;
    if (banned_.count(peer)) continue;  // never relay to a banned peer
    out.push_back({peer, message});
  }
  return out;
}

std::vector<GossipOverlay::Relay> GossipOverlay::originate(const Bytes& message) {
  mark_seen(message_id(message));  // never re-accept our own message
  return flood(message, std::nullopt);
}

std::vector<GossipOverlay::Relay> GossipOverlay::on_receive(std::uint32_t from,
                                                            const Bytes& message) {
  if (banned_.count(from)) return {};       // ignore anything from a banned peer
  if (!mark_seen(message_id(message))) {
    return {};  // already seen → drop (stops loops and floods)
  }
  return flood(message, from);
}

bool GossipOverlay::penalize(std::uint32_t peer, int weight) {
  if (peer == self_ || banned_.count(peer)) return false;
  int& acc = misbehavior_[peer];
  acc += weight;
  if (acc >= cfg_.ban_threshold) {
    ban(peer);
    return true;
  }
  return false;
}

void GossipOverlay::ban(std::uint32_t peer) {
  banned_.insert(peer);
  peers_.erase(peer);  // stop relaying to it; on_receive already ignores it
}

int GossipOverlay::score(std::uint32_t peer) const {
  auto it = misbehavior_.find(peer);
  return it == misbehavior_.end() ? 0 : it->second;
}

}  // namespace blockchainer::p2p
