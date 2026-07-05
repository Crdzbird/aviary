// Formal genesis: a GenesisSpec is a canonical, hashable description of a chain's
// initial conditions. Same spec → same genesis hash, same initial state root,
// same chain params; a produced block then extends that state.
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }

const AccountStateMachine& as_account(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}

GenesisSpec sample() {
  GenesisSpec g;
  g.chain_id = 7;
  g.state_model = StateModel::kAccount;
  g.timestamp = 1000;
  g.flat_fee = 5;
  g.max_block_txs = 100;
  g.max_block_bytes = 1u << 20;
  g.accounts = {
      {crypto::address_of(view(ed(1).public_key)), 1000},
      {crypto::address_of(view(ed(2).public_key)), 500},
  };
  g.validators = {{bls(10).public_key, 1}, {bls(11).public_key, 1}};
  return g;
}
}  // namespace

int main() {
  init();

  std::printf("== deterministic: same spec → same hash, encoding, state root ==\n");
  {
    GenesisSpec a = sample();
    GenesisSpec b = sample();
    CHECK(a.encode() == b.encode());
    CHECK(a.hash() == b.hash());

    auto la = ledger_from_genesis(a);
    auto lb = ledger_from_genesis(b);
    CHECK(la->tip().state_root == lb->tip().state_root);
    CHECK(la->tip().height == 0);
    CHECK(la->tip().timestamp == 1000);
  }

  std::printf("== account balances + chain params come from the spec ==\n");
  {
    GenesisSpec g = sample();
    auto l = ledger_from_genesis(g);
    const Hash alice = crypto::address_of(view(ed(1).public_key));
    const Hash bob = crypto::address_of(view(ed(2).public_key));
    CHECK(as_account(*l).account(alice).balance == 1000);
    CHECK(as_account(*l).account(bob).balance == 500);
    CHECK(l->spec().chain_id == 7);
    CHECK(l->spec().flat_fee == 5);
    CHECK(l->spec().max_block_txs == 100);
  }

  std::printf("== account ordering is canonical (hash is order-independent) ==\n");
  {
    GenesisSpec g1 = sample();
    GenesisSpec g2 = sample();
    std::swap(g2.accounts[0], g2.accounts[1]);  // different insertion order
    CHECK(g1.hash() == g2.hash());              // same canonical commitment
  }

  std::printf("== a different balance changes the genesis hash ==\n");
  {
    GenesisSpec g1 = sample();
    GenesisSpec g2 = sample();
    g2.accounts[0].balance += 1;
    CHECK(g1.hash() != g2.hash());
  }

  std::printf("== encode/decode round-trip ==\n");
  {
    GenesisSpec g = sample();
    Bytes enc = g.encode();
    GenesisSpec back = GenesisSpec::decode_all(view(enc));
    CHECK(back.encode() == enc);
    CHECK(back.hash() == g.hash());
    CHECK(back.chain_id == 7);
    CHECK(back.validators.size() == 2);
    CHECK(back.accounts.size() == 2);
  }

  std::printf("== the validator set is recoverable from the spec ==\n");
  {
    GenesisSpec g = sample();
    consensus::ValidatorSet vs = g.validator_set();
    CHECK(vs.size() == 2);
    CHECK(vs.total_power() == 2);
  }

  std::printf("== the genesis ledger accepts a block extending it ==\n");
  {
    GenesisSpec g = sample();
    auto l = ledger_from_genesis(g);
    Block b = l->build_block({}, g.timestamp + 1);
    CHECK(l->process_block(b).ok);
    CHECK(l->tip().height == 1);
  }

  return bctest::summary();
}
