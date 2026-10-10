#include "aviary/chain_assembler.hpp"

#include <map>
#include <mutex>
#include <stdexcept>

#include "aviary/bft_engine.hpp"
#include "aviary/pow_engine.hpp"
#include "aviary/vrf_pos.hpp"

namespace aviary::node {

namespace {

struct Registries {
  std::mutex mu;
  std::map<std::string, EngineEntry> engines;
  std::map<std::string, StateFactory> states;
  std::map<std::string, ProposerFactory> proposers;
};
Registries& regs() {
  static Registries r;
  return r;
}

// ── Built-ins ────────────────────────────────────────────────────────────────
// Registered once, lazily, so the four shipped engines / two state models / two
// proposer policies are always nameable. Custom components simply add to the
// same maps.
void register_builtins_locked(Registries& r) {
  // Engines.
  r.engines["bft"] = {[](BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
                        return std::make_unique<consensus::BftEngine>(c.vset, std::move(c.ledger));
                      },
                      /*grind_proof=*/false};
  r.engines["pow"] = {[](BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
                        // params = a 32-byte difficulty target; absent → easiest (all 0xFF).
                        Hash target;
                        target.fill(0xFF);
                        if (c.blueprint && c.blueprint->component_params.size() == 32) {
                          for (std::size_t i = 0; i < 32; ++i)
                            target[i] = c.blueprint->component_params[i];
                        }
                        return std::make_unique<consensus::PowEngine>(target, std::move(c.ledger));
                      },
                      false};
  r.engines["pos"] = {[](BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
                        consensus::ProposerPolicy p =
                            c.proposer ? c.proposer : consensus::stake_weighted_proposer;
                        return std::make_unique<consensus::PosEngine>(c.vset, c.me, c.bls_key,
                                                                      std::move(c.ledger), p);
                      },
                      false};
  r.engines["vrf-pos"] = {[](BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
                            if (c.vrf_secret.empty() || c.vrf_pubkeys.size() != c.vset.size()) {
                              throw std::runtime_error(
                                  "chain-assembler: engine 'vrf-pos' needs a per-node VRF secret "
                                  "and a validator-aligned VRF key registry");
                            }
                            return std::make_unique<consensus::VrfPosEngine>(
                                c.vset, c.vrf_pubkeys, c.me, c.bls_key, c.vrf_secret,
                                std::move(c.ledger));
                          },
                          /*grind_proof=*/true};
  // State models — the id must match how the genesis is seeded.
  r.states["account"] = [](const GenesisSpec& g) {
    if (g.state_model != StateModel::kAccount)
      throw std::runtime_error("chain-assembler: state 'account' but genesis is seeded as UTXO");
    return g.build_state();
  };
  r.states["utxo"] = [](const GenesisSpec& g) {
    if (g.state_model != StateModel::kUtxo)
      throw std::runtime_error("chain-assembler: state 'utxo' but genesis is seeded as account");
    return g.build_state();
  };
  // Proposer policies.
  r.proposers["stake-weighted"] = [] { return consensus::ProposerPolicy(consensus::stake_weighted_proposer); };
  r.proposers["round-robin"] = [] {
    return consensus::ProposerPolicy([](std::uint64_t height, const consensus::ValidatorSet& v) {
      return static_cast<std::uint32_t>(height % v.size());
    });
  };
}

Registries& ready() {
  Registries& r = regs();
  static std::once_flag once;
  std::call_once(once, [&] {
    std::lock_guard<std::mutex> lk(r.mu);
    register_builtins_locked(r);
  });
  return r;
}

template <class M>
std::vector<std::string> keys(const M& m) {
  std::vector<std::string> out;
  for (const auto& [k, v] : m) out.push_back(k);
  return out;
}

}  // namespace

// ── ComponentRegistry ────────────────────────────────────────────────────────

void ComponentRegistry::register_engine(const std::string& id, EngineFactory make,
                                        bool grind_proof) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  r.engines[id] = EngineEntry{std::move(make), grind_proof};
}
void ComponentRegistry::register_state(const std::string& id, StateFactory make) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  r.states[id] = std::move(make);
}
void ComponentRegistry::register_proposer(const std::string& id, ProposerFactory make) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  r.proposers[id] = std::move(make);
}
std::vector<std::string> ComponentRegistry::engine_ids() {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  return keys(r.engines);
}
std::vector<std::string> ComponentRegistry::state_ids() {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  return keys(r.states);
}
std::vector<std::string> ComponentRegistry::proposer_ids() {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  return keys(r.proposers);
}
const EngineEntry* ComponentRegistry::find_engine(const std::string& id) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  auto it = r.engines.find(id);
  return it == r.engines.end() ? nullptr : &it->second;
}
const StateFactory* ComponentRegistry::find_state(const std::string& id) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  auto it = r.states.find(id);
  return it == r.states.end() ? nullptr : &it->second;
}
const ProposerFactory* ComponentRegistry::find_proposer(const std::string& id) {
  Registries& r = ready();
  std::lock_guard<std::mutex> lk(r.mu);
  auto it = r.proposers.find(id);
  return it == r.proposers.end() ? nullptr : &it->second;
}

// ── ChainAssembler ───────────────────────────────────────────────────────────

ChainBlueprint ChainAssembler::default_blueprint(const GenesisSpec& genesis) {
  const AssuranceProfile profile = AssuranceProfile::from_genesis(genesis);
  ChainBlueprint b;
  b.consensus_id =
      profile.consensus() == AssuranceProfile::Consensus::kVrf ? "vrf-pos" : "pos";
  b.state_model_id = genesis.state_model == StateModel::kAccount ? "account" : "utxo";
  return b;
}

AssembledChain ChainAssembler::assemble(const GenesisSpec& genesis, std::uint32_t me,
                                        crypto::KeyPair bls_key, Bytes vrf_secret,
                                        std::vector<Bytes> vrf_pubkeys) {
  // 1. The posture check every boot already runs (unknown id / inconsistent sigs → throw).
  const AssuranceProfile profile = AssuranceProfile::from_genesis(genesis);

  // 2. The mixture: committed in genesis, else the posture-driven default.
  ChainBlueprint bp = ChainBlueprint::from_genesis(genesis).value_or(default_blueprint(genesis));

  // 3. State model → genesis state → ledger (the id must match the seeding).
  const StateFactory* sf = ComponentRegistry::find_state(bp.state_model_id);
  if (!sf) throw std::runtime_error("chain-assembler: unknown state model id '" + bp.state_model_id + "'");
  auto ledger = std::make_unique<Ledger>(genesis.chain_spec(), (*sf)(genesis), genesis.timestamp);

  // 4. Proposer policy (optional).
  consensus::ProposerPolicy proposer;
  if (!bp.proposer_id.empty()) {
    const ProposerFactory* pf = ComponentRegistry::find_proposer(bp.proposer_id);
    if (!pf) throw std::runtime_error("chain-assembler: unknown proposer id '" + bp.proposer_id + "'");
    proposer = (*pf)();
  }

  // 5. Engine — and the posture must permit it: a grind-proof posture cannot
  //    run a predictable-leader engine.
  const EngineEntry* ee = ComponentRegistry::find_engine(bp.consensus_id);
  if (!ee) throw std::runtime_error("chain-assembler: unknown consensus id '" + bp.consensus_id + "'");
  if (profile.consensus() == AssuranceProfile::Consensus::kVrf && !ee->grind_proof) {
    throw std::runtime_error("chain-assembler: the declared posture mandates grind-proof leader "
                             "election, but engine '" + bp.consensus_id + "' is not grind-proof");
  }

  // Aggregate-initialised in member order (ValidatorSet validates at
  // construction, so BootContext has no default constructor).
  BootContext ctx{&genesis,
                  &bp,
                  std::move(ledger),
                  genesis.validator_set(),
                  me,
                  std::move(bls_key),
                  std::move(vrf_secret),
                  std::move(vrf_pubkeys),
                  std::move(proposer)};
  std::unique_ptr<consensus::ConsensusEngine> engine = ee->make(ctx);
  if (!engine) throw std::runtime_error("chain-assembler: factory for '" + bp.consensus_id + "' returned no engine");
  return AssembledChain{std::move(bp), profile, std::move(engine)};
}

}  // namespace aviary::node
