#pragma once

#include <cstdint>
#include <vector>

#include "blockchainer/genesis.hpp"
#include "blockchainer/types.hpp"

/// The single safety knob (ARCHITECTURE §: security posture). A blockchain is
/// only as safe as its weakest configured seam — strong threshold custody over a
/// plaintext transport, or PQ signatures behind a grindable proposer, buys
/// little. An [AssuranceProfile] fixes ONE named posture and derives a
/// consistent tier at every seam from it: transaction signatures, custody,
/// transport, consensus, and mempool.
///
/// The profile's id is **committed into the genesis hash** (a field on
/// [GenesisSpec]), so every node provably agrees on the posture — a node cannot
/// silently join with a weaker configuration, exactly as cryptolib binds a suite
/// id into its PQ envelopes. `from_genesis` reconstructs the profile AND checks
/// the genesis's own parameters (its accepted signature algorithms) match what
/// the declared level mandates, so a spec that claims a strong level but permits
/// weak signatures is rejected as inconsistent.
namespace blockchainer {

class AssuranceProfile {
 public:
  /// The posture levels, strongest last. The numeric value is the on-genesis id.
  enum class Level : std::uint16_t {
    kStandard = 1,  ///< classical + PQ-hybrid sigs, mutual-auth transport, stake-weighted PoS
    kHardened = 2,  ///< + hash-based sigs, ratcheted transport, VRF PoS, threshold custody
    kFortress = 3,  ///< hash-based sigs only, ratcheted transport, VRF PoS, threshold custody, sealed mempool
  };

  /// Which transport tier the posture mandates (see p2p::SecureSession /
  /// ArmoredChannel).
  enum class Transport { kMutual, kRatcheted };
  /// Which leader-election tier (see consensus PosEngine / VrfPosEngine).
  enum class Consensus { kStakeWeighted, kVrf };

  static AssuranceProfile standard() { return AssuranceProfile(Level::kStandard); }
  static AssuranceProfile hardened() { return AssuranceProfile(Level::kHardened); }
  static AssuranceProfile fortress() { return AssuranceProfile(Level::kFortress); }

  Level level() const { return level_; }
  std::uint16_t id() const { return static_cast<std::uint16_t>(level_); }

  /// The exact set of signature schemes this posture permits on-chain — feeds
  /// `GenesisSpec::accepted_sig_algos` / `ChainSpec`. Ascending, canonical.
  std::vector<std::uint8_t> required_sig_algos() const;

  Transport transport() const;
  Consensus consensus() const;
  /// Whether validator/account keys must use threshold (FROST) custody.
  bool threshold_custody() const;
  /// Whether transactions must be submitted sealed to the proposer (anti-MEV).
  bool sealed_mempool() const;

  /// Stamp this posture into [g]: sets `assurance_profile` and overwrites
  /// `accepted_sig_algos` with the mandated set, so the posture becomes part of
  /// the chain's identity (its genesis hash).
  void apply_to(GenesisSpec& g) const;

  /// The posture a genesis declares. Throws if the id is unknown OR if the
  /// genesis's accepted signature set does not match what the level mandates
  /// (an inconsistent spec — a strong claim over weak parameters).
  static AssuranceProfile from_genesis(const GenesisSpec& g);

  bool operator==(const AssuranceProfile&) const = default;

 private:
  explicit AssuranceProfile(Level l) : level_(l) {}
  Level level_;
};

}  // namespace blockchainer
