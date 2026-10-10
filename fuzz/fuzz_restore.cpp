// libFuzzer harness for the persistence and networking decoders of the C++
// core: a node snapshot, an already signed transaction, and a gossip payload
// fed to a networked node. Each must be accepted or rejected with an
// exception; a crash or a sanitizer report is a bug.
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>

#include "aviary/account_state.hpp"
#include "aviary/genesis.hpp"
#include "aviary/networked_node.hpp"
#include "aviary/node/demo_node.hpp"
#include "aviary/version.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

struct Fixture {
  node::DemoNode node{77};
  std::unique_ptr<p2p::NetworkedNode> net;
  Fixture() {
    init();
    auto key = crypto::bls_keygen_from_ikm(view(Bytes(32, 0x61)));
    consensus::ValidatorSet vset(std::vector<consensus::Validator>{{key.public_key, 1}});
    auto genesis = std::make_unique<AccountStateMachine>();
    auto ledger = std::make_unique<Ledger>(ChainSpec{42, StateModel::kAccount}, std::move(genesis));
    net = std::make_unique<p2p::NetworkedNode>(42, vset, 0, key, std::move(ledger), 1);
    net->add_peer(1);
    (void)net->start();
  }
};
Fixture& fixture() {
  static Fixture f;
  return f;
}

template <class F>
void attempt(F&& f) {
  try {
    f();
  } catch (const std::exception&) {
  }
}
}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  Fixture& f = fixture();
  codec::ByteView v(data, size);
  attempt([&] { (void)node::DemoNode::restore(v); });
  attempt([&] { (void)f.node.submit_encoded(v); });
  attempt([&] { (void)f.net->on_gossip(1, Bytes(data, data + size)); });
  return 0;
}
