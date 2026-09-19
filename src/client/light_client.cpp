#include "blockchainer/light_client.hpp"

namespace blockchainer::client {

LightClient::Result LightClient::accept(const BlockHeader& h,
                                        const consensus::CommitCertificate& cert) {
  if (h.chain_id != chain_id_) return Result::failure("chain_id mismatch");
  if (h.height != tip_.height + 1) return Result::failure("non-sequential height");
  if (h.parent_hash != tip_.hash()) return Result::failure("parent hash mismatch");
  if (cert.height != h.height) return Result::failure("certificate height mismatch");
  if (cert.block_id != h.hash()) return Result::failure("certificate is for a different header");

  if (consensus::CommitError ce = consensus::verify_commit(chain_id_, vset_, cert);
      !ce.ok) {
    return Result::failure("invalid commit certificate: " + ce.error);
  }

  // The header must bind to the set in force at its height and carry a valid
  // last commit for the current tip under the set that certified THAT — so a
  // follower detects an unannounced validator-set swap and confirms every
  // ancestor was finalized, not merely linked. An announced change (checked
  // well-formed here) takes effect for the header after this one.
  if (consensus::CommitError ce =
          consensus::validate_consensus_data(chain_id_, vset_, tip_vset_, h, tip_.hash());
      !ce.ok) {
    return Result::failure("invalid consensus data: " + ce.error);
  }

  tip_vset_ = vset_;
  if (auto next = consensus::announced_validator_set(h)) vset_ = std::move(*next);
  tip_ = h;  // header chain verified up to here, no state needed
  return Result::success();
}

}  // namespace blockchainer::client
