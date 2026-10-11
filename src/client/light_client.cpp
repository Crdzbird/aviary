#include <functional>
#include <optional>
#include "aviary/light_client.hpp"
#include <vector>
#include <string>

namespace aviary::client {

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
  // last commit for the current tip under the set that certified THAT - so a
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

void LightClient::set_trust_level(std::uint32_t numerator, std::uint32_t denominator) {
  if (denominator == 0 || numerator >= denominator) return;  // keep the previous (sane) level
  trust_num_ = numerator;
  trust_den_ = denominator;
}

LightClient::Result LightClient::check_skip(const BlockHeader& h, const consensus::CommitCertificate& cert,
                                            const consensus::ValidatorSet& h_set) const {
  if (h.chain_id != chain_id_) return Result::failure("chain_id mismatch");
  if (h.height <= tip_.height) return Result::failure("header is not ahead of the trusted tip");
  if (max_skip_ != 0 && h.height - tip_.height > max_skip_) return Result::failure("skip exceeds max_skip");
  if (cert.height != h.height) return Result::failure("certificate height mismatch");
  if (cert.block_id != h.hash()) return Result::failure("certificate is for a different header");

  // 1. The header must commit the set we were handed for it.
  consensus::ConsensusData cd;
  try {
    cd = consensus::ConsensusData::decode_all(codec::ByteView(h.consensus_data.data(), h.consensus_data.size()));
  } catch (...) {
    return Result::failure("malformed consensus data");
  }
  if (cd.validator_set_hash != h_set.hash()) return Result::failure("header does not commit the supplied validator set");
  if (cd.next_validators) {
    try { (void)consensus::ValidatorSet(*cd.next_validators); }
    catch (const std::exception& e) { return Result::failure(std::string("invalid next validator set: ") + e.what()); }
  }
  // 2. h's own finality rule under its own set.
  if (consensus::CommitError ce = consensus::verify_commit(chain_id_, h_set, cert); !ce.ok)
    return Result::failure("invalid commit certificate: " + ce.error);
  // 3. Overlap with the set WE trust: signers' power counted in OUR set.
  std::uint64_t overlap = 0;
  for (std::uint32_t idx : cert.signers) {
    if (idx >= h_set.size()) return Result::failure("signer index out of range");
    if (auto mine = vset_.index_of(h_set.at(idx).bls_pubkey)) overlap += vset_.at(*mine).power;
  }
  // overlap / total > num / den  <=>  overlap * den > total * num  (no floats, no rounding)
  const unsigned __int128 lhs = static_cast<unsigned __int128>(overlap) * trust_den_;
  const unsigned __int128 rhs = static_cast<unsigned __int128>(vset_.total_power()) * trust_num_;
  if (!(lhs > rhs)) return Result::thin_overlap("insufficient overlap with the trusted validator set");
  return Result::success();
}

LightClient::Result LightClient::accept_skipping(const BlockHeader& h, const consensus::CommitCertificate& cert,
                                                 const consensus::ValidatorSet& h_set) {
  Result r = check_skip(h, cert, h_set);
  if (!r.ok) return r;
  tip_vset_ = h_set;
  vset_ = consensus::announced_validator_set(h).value_or(h_set);
  tip_ = h;
  return Result::success();
}

LightClient::Result LightClient::sync_to(
    const Checkpoint& target,
    const std::function<std::optional<Checkpoint>(std::uint64_t height)>& fetch) {
  if (target.header.height <= tip_.height) return Result::failure("target is not ahead of the trusted tip");
  // Iterative bisection: keep a stack of pending targets; try the top; on thin
  // overlap push the midpoint checkpoint (fetched) and retry; on acceptance pop.
  std::vector<Checkpoint> pending{target};
  std::size_t fetches = 0;
  while (!pending.empty()) {
    const Checkpoint& cp = pending.back();
    Result r = accept_skipping(cp.header, cp.cert, cp.validators);
    if (r.ok) { pending.pop_back(); continue; }
    if (!r.insufficient_overlap) return r;  // hard failure: malformed / mis-certified / not ahead
    const std::uint64_t lo = tip_.height, hi = cp.header.height;
    if (hi - lo <= 1) return Result::failure("insufficient overlap and no intermediate header to bisect to");
    const std::uint64_t mid = lo + (hi - lo) / 2;
    if (++fetches > 128) return Result::failure("bisection exceeded its fetch budget");
    std::optional<Checkpoint> m = fetch(mid);
    if (!m) return Result::failure("checkpoint unavailable at height " + std::to_string(mid));
    if (m->header.height != mid) return Result::failure("fetched checkpoint has the wrong height");
    pending.push_back(std::move(*m));
  }
  return Result::success();
}

}  // namespace aviary::client
