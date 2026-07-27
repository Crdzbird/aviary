#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "blockchainer/assurance.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/pos_engine.hpp"
#include "blockchainer/vrf_pos.hpp"

/// A validator assembled from a profiled genesis (ARCHITECTURE §: node boot).
/// This is the integration seam that turns the [AssuranceProfile] from a
/// *description* of a security posture into an *enforced* one: `boot` reads the
/// profile committed in the genesis, validates the genesis against it, and then
/// wires the concrete classes that posture mandates — the consensus engine
/// (stake-weighted PoS vs grind-proof VRF-PoS), the ledger's signature-scheme
/// gate, and the mempool policy. A node cannot boot at a posture its genesis
/// does not declare, and cannot silently run a weaker engine than its peers.
///
/// It hides *which* engine backs it: callers `propose` / `seal` / `submit`
/// uniformly (both PoS families share the same seal + `ConsensusEngine` seam),
/// so the pipeline above is written once regardless of tier.
namespace blockchainer::node {

class ProfiledValidator {
 public:
  /// Boot this node (validator index [me], BLS key [bls_key]) from [genesis].
  /// Validates the genesis against its declared [AssuranceProfile] and builds
  /// the mandated engine over a ledger seeded from the genesis. If the profile
  /// mandates VRF election, [vrf_secret] and [vrf_pubkeys] (index-aligned with
  /// the validator set) are required. Throws on an inconsistent/unknown profile
  /// or missing VRF material.
  static ProfiledValidator boot(const GenesisSpec& genesis, std::uint32_t me,
                                crypto::KeyPair bls_key, Bytes vrf_secret = {},
                                std::vector<Bytes> vrf_pubkeys = {});

  const AssuranceProfile& profile() const { return profile_; }
  const Ledger& ledger() const { return engine().ledger(); }
  std::uint64_t height() const { return engine().height(); }
  const BlockHeader& tip() const { return engine().tip(); }

  /// Whether transactions on this chain must be submitted sealed (fortress).
  bool sealed_mempool() const { return profile_.sealed_mempool(); }
  /// Whether account/validator keys should use threshold custody.
  bool threshold_custody() const { return profile_.threshold_custody(); }

  // ── Uniform consensus ops (dispatch to whichever engine the profile chose) ──

  /// Assemble the next candidate block (throws if this node isn't the leader /
  /// eligible for tip+1).
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) {
    return engine().propose(std::move(txs), timestamp);
  }
  /// The leader's justification signature over [block] (identical for both PoS
  /// tiers).
  Bytes seal(const Block& block) const;
  /// Verify + apply a justified block.
  BlockError submit(const Block& block, codec::ByteView justification) {
    return engine().submit(block, justification);
  }

 private:
  ProfiledValidator(AssuranceProfile profile, std::unique_ptr<consensus::PosEngine> pos,
                    std::unique_ptr<consensus::VrfPosEngine> vrf)
      : profile_(profile), pos_(std::move(pos)), vrf_(std::move(vrf)) {}

  consensus::ConsensusEngine& engine();
  const consensus::ConsensusEngine& engine() const;

  AssuranceProfile profile_;
  std::unique_ptr<consensus::PosEngine> pos_;      // set iff consensus() == kStakeWeighted
  std::unique_ptr<consensus::VrfPosEngine> vrf_;   // set iff consensus() == kVrf
};

}  // namespace blockchainer::node
