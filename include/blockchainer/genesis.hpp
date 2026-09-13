#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/state.hpp"
#include "blockchainer/types.hpp"

/// Formal genesis (ARCHITECTURE §0). A [GenesisSpec] is the canonical, hashable
/// description of a chain's initial conditions — its parameters, its starting
/// balances, and its validator set. Two nodes that load the same spec derive the
/// same genesis hash, the same initial state root, and the same chain params, so
/// "which chain is this" has a single, verifiable answer instead of the ad-hoc
/// `credit_genesis` / `add_genesis_utxo` seeding used before.
namespace blockchainer {

/// An account funded at genesis (account model).
struct GenesisAccount {
  Hash address{};
  codec::u128 balance = 0;
};

/// An unspent output created at genesis (UTXO model). Order is significant — each
/// output is assigned a genesis outpoint by its position.
struct GenesisUtxo {
  Hash owner{};
  codec::u128 amount = 0;
};

/// The complete, canonical genesis of a chain.
struct GenesisSpec {
  std::uint64_t chain_id = 0;
  StateModel state_model = StateModel::kAccount;
  std::uint64_t timestamp = 0;

  // Chain parameters (mirror ChainSpec).
  std::vector<std::uint8_t> accepted_sig_algos{};
  codec::u128 flat_fee = 0;
  std::uint32_t max_block_txs = 0;
  std::uint64_t max_block_bytes = 0;

  // The network-wide security posture (an AssuranceProfile::Level id; 0 =
  // unspecified). Committed into the genesis hash, so every node provably agrees
  // on the tier at every seam — see assurance.hpp.
  std::uint16_t assurance_profile = 0;

  // An optional anonymous-membership policy (docs/design/CREDENTIAL_GATE.md).
  // Empty = permissionless (no admission gate). When set, these are the canonical
  // `consensus::CredentialPolicy` bytes — the issuer key, attribute schema, and
  // tier threshold — committed into the genesis hash so every node provably
  // agrees on who may join. ORTHOGONAL to `assurance_profile` (§10.4): the crypto
  // posture and the permissioned/permissionless axis are independent, so a chain
  // may be any posture × either membership mode. Reconstruct the policy with
  // `consensus::CredentialPolicy::from_genesis`.
  Bytes membership_policy{};

  // An optional composition blueprint (chain_blueprint.hpp): WHICH consensus
  // engine, state model, and proposer policy assemble this chain, each named by
  // a stable component id, plus their canonical params. Empty = the default,
  // posture-driven assembly. Committed into the genesis hash so the MIXTURE is
  // chain identity — a node cannot boot a different mix and still be this chain.
  // Reconstruct with `ChainBlueprint::from_genesis`; assemble with ChainAssembler.
  Bytes chain_blueprint{};

  // Initial state. Use `accounts` for the account model, `utxos` for the UTXO
  // model (the other stays empty).
  std::vector<GenesisAccount> accounts;
  std::vector<GenesisUtxo> utxos;

  // The initial validator set (each {48-byte BLS pubkey, power}).
  std::vector<consensus::Validator> validators;

  /// Canonical encoding — a fixed byte layout independent of map/iteration order.
  Bytes encode() const;
  static GenesisSpec decode_all(codec::ByteView bytes);

  /// The genesis hash: a domain-separated commitment to the whole spec. The
  /// chain's identity.
  Hash hash() const;

  /// The runtime chain parameters derived from this genesis.
  ChainSpec chain_spec() const;

  /// The initial validator set (throws if malformed — see ValidatorSet).
  consensus::ValidatorSet validator_set() const { return consensus::ValidatorSet(validators); }

  /// Build the initial state machine carrying the genesis flat fee: an account
  /// state credited with the genesis accounts, or a UTXO state seeded with the
  /// genesis outputs, per [state_model].
  std::unique_ptr<StateMachine> build_state() const;
};

/// Construct a fresh ledger seeded from [spec] (account model).
std::unique_ptr<Ledger> ledger_from_genesis(const GenesisSpec& spec);

}  // namespace blockchainer
