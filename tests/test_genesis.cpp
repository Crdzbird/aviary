// Formal genesis: a GenesisSpec is a canonical, hashable description of a chain's
// initial conditions. Same spec → same genesis hash, same initial state root,
// same chain params; a produced block then extends that state.
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/assurance.hpp"
#include "aviary/credential_gate.hpp"
#include "aviary/genesis.hpp"
#include "aviary/utxo_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

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

  std::printf("== UTXO genesis: outputs seeded in order, spendable, deterministic ==\n");
  {
    GenesisSpec g;
    g.chain_id = 9;
    g.state_model = StateModel::kUtxo;
    g.timestamp = 1;
    const Hash alice = crypto::address_of(view(ed(1).public_key));
    const Hash bob = crypto::address_of(view(ed(2).public_key));
    g.utxos = {{alice, 1000}, {bob, 250}, {alice, 40}};

    auto l = ledger_from_genesis(g);
    const auto& u = dynamic_cast<const UtxoStateMachine&>(l->state());
    CHECK(u.utxo_count() == 3);
    // Outpoints are assigned by order: index 0,1,2 under the zero txid.
    CHECK(u.get(UtxoStateMachine::Outpoint{crypto::zero_hash(), 0}).owner == alice);
    CHECK(u.get(UtxoStateMachine::Outpoint{crypto::zero_hash(), 0}).amount == 1000);
    CHECK(u.get(UtxoStateMachine::Outpoint{crypto::zero_hash(), 1}).owner == bob);
    CHECK(u.get(UtxoStateMachine::Outpoint{crypto::zero_hash(), 2}).amount == 40);

    // Determinism + round-trip.
    CHECK(GenesisSpec::decode_all(view(g.encode())).hash() == g.hash());
    auto l2 = ledger_from_genesis(g);
    CHECK(l->tip().state_root == l2->tip().state_root);

    // Order is significant: reordering the outputs changes the genesis hash.
    GenesisSpec g2 = g;
    std::swap(g2.utxos[0], g2.utxos[1]);
    CHECK(g2.hash() != g.hash());
  }

  // ── Membership policy committed into the genesis hash (§10.4, orthogonal) ────
  {
    GenesisSpec open = sample();  // permissionless: no membership policy
    CHECK(open.membership_policy.empty());
    CHECK(!consensus::CredentialPolicy::from_genesis(open).has_value());

    // Build a policy and commit it into a copy of the same genesis.
    auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x5A)));
    consensus::CredentialPolicy policy;
    policy.chain_id = open.chain_id;
    policy.issuer_public_key = issuer.public_key;
    policy.header = Bytes{'m', 'e', 'm', 'b', 'e', 'r'};
    policy.min_tier = 2;

    GenesisSpec gated = sample();
    policy.commit_to(gated);

    // Committing membership changes chain identity (permissioned ≠ permissionless).
    CHECK(!gated.membership_policy.empty());
    CHECK(gated.hash() != open.hash());
    // It round-trips through the genesis codec, hash-stable.
    CHECK(GenesisSpec::decode_all(view(gated.encode())).hash() == gated.hash());
    // And reconstructs to exactly the committed policy.
    auto back = consensus::CredentialPolicy::from_genesis(gated);
    CHECK(back.has_value());
    CHECK(*back == policy);
    // A node can build a working gate straight from genesis.
    consensus::CredentialGate gate(*back);
    CHECK(gate.policy().hash() == policy.hash());

    // Orthogonal to the assurance posture (§10.4): the two axes are independent.
    // A fortress chain can be permissionless; a standard chain can be permissioned.
    GenesisSpec fortress_open = sample();
    AssuranceProfile::fortress().apply_to(fortress_open);
    CHECK(fortress_open.assurance_profile == 3);
    CHECK(fortress_open.membership_policy.empty());  // fortress + permissionless

    GenesisSpec standard_gated = sample();
    policy.commit_to(standard_gated);  // standard posture + permissioned
    CHECK(standard_gated.assurance_profile == 0);
    CHECK(!standard_gated.membership_policy.empty());
    // Committing membership leaves the assurance field untouched, and vice versa.
    AssuranceProfile::fortress().apply_to(standard_gated);
    CHECK(standard_gated.assurance_profile == 3 && !standard_gated.membership_policy.empty());
    CHECK(consensus::CredentialPolicy::from_genesis(standard_gated).has_value());
  }

  return bctest::summary();
}
