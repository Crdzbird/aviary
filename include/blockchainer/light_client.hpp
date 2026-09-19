#pragma once

#include <cstdint>
#include <string>

#include "blockchainer/consensus.hpp"
#include "blockchainer/types.hpp"

/// Light client (ARCHITECTURE §10): follows the header chain and verifies each
/// header's commit certificate against the validator set, **without** holding
/// transactions or full state. Because >2/3 of voting power signs the header id
/// (which commits tx_root and state_root), a verified header lets the client
/// trust those roots — and thus verify inclusion proofs against them.
namespace blockchainer::client {

class LightClient {
 public:
  struct Result {
    bool ok = false;
    std::string error;
    static Result success() { return {true, {}}; }
    static Result failure(std::string e) { return {false, std::move(e)}; }
  };

  /// Start from a trusted header (e.g. genesis) and the validator set in force
  /// for the NEXT header (which also certified the trusted tip).
  LightClient(std::uint64_t chain_id, consensus::ValidatorSet vset,
              BlockHeader trusted_tip)
      : chain_id_(chain_id), vset_(vset), tip_vset_(std::move(vset)), tip_(std::move(trusted_tip)) {}
  /// Resume mid-chain across a validator-set change: [vset] is in force for the
  /// next header, [tip_vset] is the set that certified [trusted_tip].
  LightClient(std::uint64_t chain_id, consensus::ValidatorSet vset, BlockHeader trusted_tip,
              consensus::ValidatorSet tip_vset)
      : chain_id_(chain_id), vset_(std::move(vset)), tip_vset_(std::move(tip_vset)), tip_(std::move(trusted_tip)) {}

  const BlockHeader& tip() const { return tip_; }
  std::uint64_t height() const { return tip_.height; }
  /// The validator set in force for the next header (follows announced changes).
  const consensus::ValidatorSet& validators() const { return vset_; }
  /// The set that certified the current tip (differs from validators() right
  /// after a rotation header).
  const consensus::ValidatorSet& tip_validators() const { return tip_vset_; }

  /// Verify [h] links to the current tip and that [cert] is a valid commit
  /// certificate for it; on success, advance the tip. Needs no block body.
  Result accept(const BlockHeader& h, const consensus::CommitCertificate& cert);

 private:
  std::uint64_t chain_id_;
  consensus::ValidatorSet vset_;      // in force for the next header
  consensus::ValidatorSet tip_vset_;  // certified the tip
  BlockHeader tip_;
};

}  // namespace blockchainer::client
