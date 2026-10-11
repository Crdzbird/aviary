#pragma once

#include <optional>
#include <string>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

namespace aviary {
struct GenesisSpec;
}  // namespace aviary

/// A declarative MIXTURE of chain components. A chain is assembled from
/// pluggable parts - a consensus engine, a state model, a proposer policy - 
/// each implemented behind an abstract seam (ConsensusEngine, StateMachine,
/// ProposerPolicy). The blueprint names WHICH implementation of each to use, by
/// a stable component id, plus canonical params they interpret.
///
/// Integrity: the blueprint's hash is committed into the genesis hash, so the
/// mixture itself is chain identity. Every node resolves the same ids through
/// the same ComponentRegistry (chain_assembler.hpp); an id a node does not know,
/// or a combination the declared assurance posture forbids, is REJECTED at boot
/// (fail-closed) rather than silently substituted. Custom components extend the
/// seams by inheritance and join the mixture by registering under an id - they
/// must be deterministic (no clock, no randomness, canonical encodings), since
/// they run inside the consensus-critical pipeline.
namespace aviary {

struct ChainBlueprint {
  std::string consensus_id;    ///< "bft" | "pow" | "pos" | "vrf-pos" | a registered custom id
  std::string state_model_id;  ///< "account" | "utxo" | a registered custom id
  std::string proposer_id;     ///< "stake-weighted" | "round-robin" | custom; "" = engine default
  Bytes component_params;      ///< canonical, interpreted by the chosen components

  /// Canonical encoding (docs/CODEC_SPEC.md conventions).
  Bytes encode() const;
  /// Strict canonical decode (rejects trailing bytes / malformed input).
  static ChainBlueprint decode_all(crypto::ByteView bytes);
  /// Domain-separated commitment: hash_tagged(kChainBlueprint, encode()).
  crypto::Hash hash() const;

  /// Stamp this mixture into [g] so it becomes part of the chain's identity - 
  /// beside (orthogonal to) the assurance posture and membership policy.
  void commit_to(GenesisSpec& g) const;
  /// The mixture a genesis commits to, or std::nullopt for the default
  /// (posture-driven) assembly. Throws if the committed bytes are malformed.
  static std::optional<ChainBlueprint> from_genesis(const GenesisSpec& g);

  bool operator==(const ChainBlueprint&) const = default;
};

}  // namespace aviary
