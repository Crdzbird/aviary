#include <memory>
#include "aviary/profiled_validator.hpp"

#include <stdexcept>

namespace aviary::node {

ProfiledValidator ProfiledValidator::boot(const GenesisSpec& genesis, std::uint32_t me,
                                          crypto::KeyPair bls_key, Bytes vrf_secret,
                                          std::vector<Bytes> vrf_pubkeys) {
  // Validate the genesis against the posture it declares — a spec that claims a
  // strong level over weak parameters, or an unknown id, is rejected here.
  const AssuranceProfile profile = AssuranceProfile::from_genesis(genesis);

  consensus::ValidatorSet vset = genesis.validator_set();
  auto ledger = ledger_from_genesis(genesis);  // carries the profile's sig-algo gate

  std::unique_ptr<consensus::PosEngine> pos;
  std::unique_ptr<consensus::VrfPosEngine> vrf;

  switch (profile.consensus()) {
    case AssuranceProfile::Consensus::kStakeWeighted:
      pos = std::make_unique<consensus::PosEngine>(std::move(vset), me, std::move(bls_key),
                                                   std::move(ledger));
      break;
    case AssuranceProfile::Consensus::kVrf:
      if (vrf_secret.empty() || vrf_pubkeys.size() != vset.size()) {
        throw std::runtime_error(
            "profiled-validator: this posture mandates VRF election — a per-node VRF secret and a "
            "validator-aligned VRF key registry are required");
      }
      vrf = std::make_unique<consensus::VrfPosEngine>(std::move(vset), std::move(vrf_pubkeys), me,
                                                      std::move(bls_key), std::move(vrf_secret),
                                                      std::move(ledger));
      break;
  }
  return ProfiledValidator(profile, std::move(pos), std::move(vrf));
}

consensus::ConsensusEngine& ProfiledValidator::engine() {
  if (pos_) return *pos_;
  return *vrf_;
}
const consensus::ConsensusEngine& ProfiledValidator::engine() const {
  if (pos_) return *pos_;
  return *vrf_;
}

Bytes ProfiledValidator::seal(const Block& block) const {
  return pos_ ? pos_->seal(block) : vrf_->seal(block);
}

}  // namespace aviary::node
