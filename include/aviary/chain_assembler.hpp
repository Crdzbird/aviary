#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "aviary/assurance.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/consensus_engine.hpp"
#include "aviary/genesis.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/state.hpp"

/// Assembles a chain from a ChainBlueprint by resolving each component id
/// through a registry of factories. This replaces a hardcoded "switch over the
/// engines we know" with an open, fail-closed one: built-ins register on first
/// use, custom subclasses of the seams register under their own id, and an id
/// nobody registered — or a mix the assurance posture forbids — throws.
namespace aviary::node {

/// Everything a component factory may draw on. [ledger] is built from the
/// genesis plus the chosen state model; the engine factory takes ownership.
struct BootContext {
  const GenesisSpec* genesis = nullptr;
  const ChainBlueprint* blueprint = nullptr;
  std::unique_ptr<Ledger> ledger;
  consensus::ValidatorSet vset;
  std::uint32_t me = 0;
  crypto::KeyPair bls_key;
  Bytes vrf_secret;
  std::vector<Bytes> vrf_pubkeys;
  consensus::ProposerPolicy proposer;  ///< resolved from blueprint.proposer_id ("" → empty)
};

using EngineFactory = std::function<std::unique_ptr<consensus::ConsensusEngine>(BootContext&)>;
/// Builds the GENESIS state for a model id (seeded from the spec).
using StateFactory = std::function<std::unique_ptr<StateMachine>(const GenesisSpec&)>;
using ProposerFactory = std::function<consensus::ProposerPolicy()>;

/// A registered engine: its factory plus whether its leader election is
/// grind-proof (unpredictable) — what a hardened/fortress posture mandates.
struct EngineEntry {
  EngineFactory make;
  bool grind_proof = false;
};

/// The open registry of pluggable components. Register custom implementations
/// once at startup, before assembling. Ids are chain identity: never rename one.
class ComponentRegistry {
 public:
  static void register_engine(const std::string& id, EngineFactory make, bool grind_proof);
  static void register_state(const std::string& id, StateFactory make);
  static void register_proposer(const std::string& id, ProposerFactory make);

  /// Discovery — what a mixture may name (built-ins + anything registered).
  static std::vector<std::string> engine_ids();
  static std::vector<std::string> state_ids();
  static std::vector<std::string> proposer_ids();

  static const EngineEntry* find_engine(const std::string& id);
  static const StateFactory* find_state(const std::string& id);
  static const ProposerFactory* find_proposer(const std::string& id);
};

/// An assembled chain: the mixture it was built from, the posture it runs
/// under, and the engine — behind the uniform ConsensusEngine seam.
struct AssembledChain {
  ChainBlueprint blueprint;
  AssuranceProfile profile;
  std::unique_ptr<consensus::ConsensusEngine> engine;
};

class ChainAssembler {
 public:
  /// Assemble from [genesis]. Uses its committed blueprint, or the posture-
  /// driven default if none. Throws on an unknown component id, a state model
  /// that does not match the genesis seeding, or an engine the declared
  /// assurance posture forbids (kVrf mandates a grind-proof engine).
  static AssembledChain assemble(const GenesisSpec& genesis, std::uint32_t me,
                                 crypto::KeyPair bls_key, Bytes vrf_secret = {},
                                 std::vector<Bytes> vrf_pubkeys = {});

  /// The mixture a genesis WITHOUT a blueprint assembles as — derived from its
  /// posture and state model, so every legacy genesis is still assemblable.
  static ChainBlueprint default_blueprint(const GenesisSpec& genesis);
};

}  // namespace aviary::node
