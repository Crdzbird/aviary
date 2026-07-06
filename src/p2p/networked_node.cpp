#include "blockchainer/networked_node.hpp"

#include <deque>

#include "blockchainer/wire.hpp"

namespace blockchainer::p2p {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

NetworkedNode::Outbound NetworkedNode::emit(const consensus::RoundDriver::Output& out) {
  Outbound o;
  o.timeouts = out.timeouts;

  // The round driver expects to receive its OWN messages back (its vote counting
  // happens on delivery, not on broadcast). Over gossip we send to peers but
  // never to ourselves, so we self-deliver each of our broadcasts here — feeding
  // it to our own driver AND originating it to the mesh. Self-delivery can emit
  // further broadcasts (e.g. our own prevote lets us reach a precommit), so we
  // drain a work-list to a fixpoint.
  std::deque<wire::Message> pending(out.broadcasts.begin(), out.broadcasts.end());
  while (!pending.empty()) {
    wire::Message m = std::move(pending.front());
    pending.pop_front();
    const Bytes payload = m.encode();

    auto relays = gossip_.originate(payload);  // marks seen → peers' echoes dedup
    o.relays.insert(o.relays.end(), relays.begin(), relays.end());

    consensus::RoundDriver::Output more = driver_.deliver(wire::as_consensus(m));
    for (auto& mm : more.broadcasts) pending.push_back(std::move(mm));
    for (const auto& t : more.timeouts) o.timeouts.push_back(t);
  }
  return o;
}

NetworkedNode::Outbound NetworkedNode::start() { return emit(driver_.start()); }

NetworkedNode::Outbound NetworkedNode::on_gossip(std::uint32_t from, const Bytes& payload) {
  if (gossip_.banned(from)) return {};  // a banned peer is silenced entirely

  // Dedup at the overlay: a message that already flooded through here is dropped
  // (this is what bounds traffic and stops loops), so the driver sees each unique
  // message exactly once.
  if (gossip_.seen(payload)) return {};

  Outbound o;
  o.relays = gossip_.on_receive(from, payload);  // marks seen; relays to peers != from

  // Decode and feed the consensus driver; any consensus it emits is gossiped.
  try {
    wire::Message m = wire::Message::decode_all(view(payload));
    if (m.type == wire::MessageType::kConsensus) {
      Outbound produced = emit(driver_.deliver(wire::as_consensus(m)));
      o.relays.insert(o.relays.end(), produced.relays.begin(), produced.relays.end());
      o.timeouts = std::move(produced.timeouts);
    }
  } catch (...) {
    // Undecodable payload = misbehavior. It already deduped (won't re-flood), and
    // a peer that keeps sending garbage crosses the ban threshold and is cut off.
    gossip_.penalize(from);
  }
  return o;
}

NetworkedNode::Outbound NetworkedNode::on_timeout(consensus::Step step, std::uint32_t round) {
  return emit(driver_.on_timeout(step, round));
}

}  // namespace blockchainer::p2p
