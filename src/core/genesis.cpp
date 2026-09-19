#include "blockchainer/genesis.hpp"

#include <algorithm>
#include <stdexcept>

#include "blockchainer/account_state.hpp"
#include "blockchainer/utxo_state.hpp"

namespace blockchainer {

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
codec::ByteView hview(const Hash& h) { return codec::ByteView(h.data(), h.size()); }

Hash to_hash(const Bytes& b) {
  Hash h{};
  for (std::size_t i = 0; i < 32; ++i) h[i] = b[i];
  return h;
}
}  // namespace

Bytes GenesisSpec::encode() const {
  codec::CodecWriter w;
  w.write_u64(chain_id);
  w.write_u8(static_cast<std::uint8_t>(state_model));
  w.write_u64(timestamp);
  w.write_list(accepted_sig_algos,
               [](codec::CodecWriter& w, std::uint8_t a) { w.write_u8(a); });
  w.write_u128(flat_fee);
  w.write_u32(max_block_txs);
  w.write_u64(max_block_bytes);
  w.write_u16(assurance_profile);
  // Accounts, sorted by address → canonical regardless of insertion order.
  std::vector<GenesisAccount> sorted_accounts = accounts;
  std::sort(sorted_accounts.begin(), sorted_accounts.end(),
            [](const GenesisAccount& a, const GenesisAccount& b) { return a.address < b.address; });
  w.write_list(sorted_accounts, [](codec::CodecWriter& w, const GenesisAccount& a) {
    w.write_fixed_bytes(hview(a.address), 32);
    w.write_u128(a.balance);
  });
  // UTXOs in the given order — the position assigns each output's genesis
  // outpoint, so order is significant and must NOT be re-sorted.
  w.write_list(utxos, [](codec::CodecWriter& w, const GenesisUtxo& u) {
    w.write_fixed_bytes(hview(u.owner), 32);
    w.write_u128(u.amount);
  });
  // Validators, in the given (index-significant) order.
  w.write_list(validators, [](codec::CodecWriter& w, const consensus::Validator& v) {
    w.write_bytes(view(v.bls_pubkey));
    w.write_u64(v.power);
  });
  // Optional membership policy (empty = permissionless). Appended last so it is
  // clearly an orthogonal, later-added posture field.
  w.write_bytes(view(membership_policy));
  w.write_bytes(view(chain_blueprint));
  w.write_bytes(view(subchain_policy));
  w.write_bytes(view(parent_binding));
  return w.take();
}

GenesisSpec GenesisSpec::decode_all(codec::ByteView bytes) {
  codec::CodecReader r(bytes);
  GenesisSpec g;
  g.chain_id = r.read_u64();
  g.state_model = static_cast<StateModel>(r.read_u8());
  g.timestamp = r.read_u64();
  g.accepted_sig_algos = r.read_list<std::uint8_t>([](codec::CodecReader& r) { return r.read_u8(); });
  g.flat_fee = r.read_u128();
  g.max_block_txs = r.read_u32();
  g.max_block_bytes = r.read_u64();
  g.assurance_profile = r.read_u16();
  g.accounts = r.read_list<GenesisAccount>([](codec::CodecReader& r) {
    GenesisAccount a;
    a.address = to_hash(r.read_fixed_bytes(32));
    a.balance = r.read_u128();
    return a;
  });
  g.utxos = r.read_list<GenesisUtxo>([](codec::CodecReader& r) {
    GenesisUtxo u;
    u.owner = to_hash(r.read_fixed_bytes(32));
    u.amount = r.read_u128();
    return u;
  });
  g.validators = r.read_list<consensus::Validator>([](codec::CodecReader& r) {
    consensus::Validator v;
    v.bls_pubkey = r.read_bytes();
    v.power = r.read_u64();
    return v;
  });
  g.membership_policy = r.read_bytes();
  g.chain_blueprint = r.read_bytes();
  g.subchain_policy = r.read_bytes();
  g.parent_binding = r.read_bytes();
  r.assert_fully_consumed();
  return g;
}

Hash GenesisSpec::hash() const {
  return crypto::hash_tagged(crypto::Domain::kGenesis, encode());
}

ChainSpec GenesisSpec::chain_spec() const {
  ChainSpec s;
  s.chain_id = chain_id;
  s.state_model = state_model;
  s.accepted_sig_algos = accepted_sig_algos;
  s.flat_fee = flat_fee;
  s.max_block_txs = max_block_txs;
  s.max_block_bytes = max_block_bytes;
  return s;
}

std::unique_ptr<StateMachine> GenesisSpec::build_state() const {
  if (state_model == StateModel::kAccount) {
    auto state = std::make_unique<AccountStateMachine>(flat_fee);
    for (const auto& a : accounts) state->credit_genesis(a.address, a.balance);
    return state;
  }
  auto state = std::make_unique<UtxoStateMachine>(flat_fee);
  for (const auto& u : utxos) {
    state->add_genesis_utxo(UtxoStateMachine::Output{u.owner, u.amount});  // order = outpoint index
  }
  return state;
}

std::unique_ptr<Ledger> ledger_from_genesis(const GenesisSpec& spec) {
  return std::make_unique<Ledger>(spec.chain_spec(), spec.build_state(), spec.timestamp);
}

}  // namespace blockchainer
