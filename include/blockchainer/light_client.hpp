#pragma once

#include <cstdint>
#include <functional>
#include <optional>
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
    /// Set only by accept_skipping: the header is well-certified by its own
    /// set but too few of OUR trusted validators signed it — fetch an
    /// intermediate header and bisect (see sync_to). Never set for a header
    /// that is malformed, mis-certified, or not ahead of the tip.
    bool insufficient_overlap = false;
    static Result success() { return {true, {}, false}; }
    static Result failure(std::string e) { return {false, std::move(e), false}; }
    static Result thin_overlap(std::string e) { return {false, std::move(e), true}; }
  };

  /// A header, its commit certificate, and the FULL validator set in force at
  /// its height (whose hash the header commits) — what a skipping client needs.
  struct Checkpoint {
    BlockHeader header;
    consensus::CommitCertificate cert;
    consensus::ValidatorSet validators;
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

  /// The OVERLAP (skipping) rule: jump to [h] at ANY height ahead of the tip
  /// without the headers in between. Accepted iff
  ///   1. h commits the hash of [h_set] (the set in force at its height),
  ///   2. [cert] is a >2/3 certificate for h under [h_set] (h's own rule), and
  ///   3. the signers of [cert] who are also in OUR trusted set hold more than
  ///      trust_numerator/trust_denominator (default 1/3) of ITS power —
  ///      with fewer than 1/3 Byzantine among the validators we already
  ///      trust, at least one honest trusted validator vouched for h.
  /// On success the tip is h, the certifying set is [h_set], and the set in
  /// force next is h's announced set if any, else [h_set]. Height tip+1 is
  /// still accepted here (a skip of one needs no linkage either).
  ///
  /// SECURITY ASSUMPTION (the "trusting period"): this rule is only sound
  /// while the validators of the trusted tip can still be held accountable —
  /// a client that trusts a tip older than the chain's unbonding period must
  /// re-bootstrap from a fresh trusted header instead. Blockchainer has no
  /// consensus clock, so the bound is the operator's: see max_skip().
  Result accept_skipping(const BlockHeader& h, const consensus::CommitCertificate& cert,
                         const consensus::ValidatorSet& h_set);

  /// Sync to [target] with bisection: try to skip straight there; when the
  /// overlap is too thin, ask [fetch] for a checkpoint at the midpoint and
  /// recurse (each accepted checkpoint refreshes the trusted set). Returns the
  /// first hard failure, or success at target. [fetch] returning nullopt for a
  /// height is a failure ("checkpoint unavailable"). Bounded: at most
  /// ~log2(distance) fetches per accepted hop, and max_skip() per hop.
  Result sync_to(const Checkpoint& target,
                 const std::function<std::optional<Checkpoint>(std::uint64_t height)>& fetch);

  /// Overlap threshold as a fraction (default 1/3). Denominator must be > 0 and
  /// numerator < denominator.
  void set_trust_level(std::uint32_t numerator, std::uint32_t denominator);
  /// The largest height jump one skip may make (0 = unbounded, the default).
  /// Operators bound this to their unbonding period in blocks.
  void set_max_skip(std::uint64_t blocks) { max_skip_ = blocks; }
  std::uint64_t max_skip() const { return max_skip_; }

 private:
  Result check_skip(const BlockHeader& h, const consensus::CommitCertificate& cert,
                    const consensus::ValidatorSet& h_set) const;

  std::uint64_t chain_id_;
  consensus::ValidatorSet vset_;      // in force for the next header
  consensus::ValidatorSet tip_vset_;  // certified the tip
  BlockHeader tip_;
  std::uint32_t trust_num_ = 1, trust_den_ = 3;
  std::uint64_t max_skip_ = 0;
};

}  // namespace blockchainer::client
