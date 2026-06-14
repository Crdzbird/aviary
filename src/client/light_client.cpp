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

  tip_ = h;  // header chain verified up to here, no state needed
  return Result::success();
}

}  // namespace blockchainer::client
