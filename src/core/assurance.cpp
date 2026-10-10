#include "aviary/assurance.hpp"

#include <stdexcept>

namespace aviary {

std::vector<std::uint8_t> AssuranceProfile::required_sig_algos() const {
  switch (level_) {
    case Level::kStandard:
      // Classical Ed25519 and the lattice PQ-hybrid.
      return {static_cast<std::uint8_t>(SigAlgo::kEd25519),
              static_cast<std::uint8_t>(SigAlgo::kHybridEd25519MlDsa)};
    case Level::kHardened:
      // Drop bare classical; require a PQ family (lattice hybrid or hash-based).
      return {static_cast<std::uint8_t>(SigAlgo::kHybridEd25519MlDsa),
              static_cast<std::uint8_t>(SigAlgo::kSlhDsa)};
    case Level::kFortress:
      // Most conservative: hash-based only (security rests on the hash alone).
      return {static_cast<std::uint8_t>(SigAlgo::kSlhDsa)};
  }
  return {};
}

AssuranceProfile::Transport AssuranceProfile::transport() const {
  return level_ == Level::kStandard ? Transport::kMutual : Transport::kRatcheted;
}

AssuranceProfile::Consensus AssuranceProfile::consensus() const {
  return level_ == Level::kStandard ? Consensus::kStakeWeighted : Consensus::kVrf;
}

bool AssuranceProfile::threshold_custody() const { return level_ != Level::kStandard; }

bool AssuranceProfile::sealed_mempool() const { return level_ == Level::kFortress; }

void AssuranceProfile::apply_to(GenesisSpec& g) const {
  g.assurance_profile = id();
  g.accepted_sig_algos = required_sig_algos();
}

AssuranceProfile AssuranceProfile::from_genesis(const GenesisSpec& g) {
  Level level;
  switch (g.assurance_profile) {
    case static_cast<std::uint16_t>(Level::kStandard):
      level = Level::kStandard;
      break;
    case static_cast<std::uint16_t>(Level::kHardened):
      level = Level::kHardened;
      break;
    case static_cast<std::uint16_t>(Level::kFortress):
      level = Level::kFortress;
      break;
    default:
      throw std::runtime_error("assurance: genesis declares an unknown profile id " +
                               std::to_string(g.assurance_profile));
  }
  AssuranceProfile p(level);
  // The declared posture must match the genesis parameters it governs — a spec
  // that claims a strong level while permitting weak signatures is a
  // misconfiguration, not a valid chain.
  if (g.accepted_sig_algos != p.required_sig_algos()) {
    throw std::runtime_error("assurance: genesis signature set does not match its declared profile");
  }
  return p;
}

}  // namespace aviary
