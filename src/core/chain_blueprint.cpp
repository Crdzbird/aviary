#include <optional>
#include "aviary/chain_blueprint.hpp"

#include "aviary/codec.hpp"
#include "aviary/genesis.hpp"

namespace aviary {

namespace {
crypto::ByteView sview(const std::string& s) {
  return crypto::ByteView(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}
std::string to_str(const Bytes& b) { return std::string(b.begin(), b.end()); }
}  // namespace

Bytes ChainBlueprint::encode() const {
  codec::CodecWriter w;
  w.write_bytes(sview(consensus_id));
  w.write_bytes(sview(state_model_id));
  w.write_bytes(sview(proposer_id));
  w.write_bytes(crypto::ByteView(component_params.data(), component_params.size()));
  return w.take();
}

ChainBlueprint ChainBlueprint::decode_all(crypto::ByteView bytes) {
  codec::CodecReader r(bytes);
  ChainBlueprint b;
  b.consensus_id = to_str(r.read_bytes());
  b.state_model_id = to_str(r.read_bytes());
  b.proposer_id = to_str(r.read_bytes());
  b.component_params = r.read_bytes();
  r.assert_fully_consumed();
  return b;
}

crypto::Hash ChainBlueprint::hash() const {
  return crypto::hash_tagged(crypto::Domain::kChainBlueprint, encode());
}

void ChainBlueprint::commit_to(GenesisSpec& g) const { g.chain_blueprint = encode(); }

std::optional<ChainBlueprint> ChainBlueprint::from_genesis(const GenesisSpec& g) {
  if (g.chain_blueprint.empty()) return std::nullopt;
  return decode_all(crypto::ByteView(g.chain_blueprint.data(), g.chain_blueprint.size()));
}

}  // namespace aviary
