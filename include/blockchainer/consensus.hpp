#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"

/// BFT consensus primitives (ARCHITECTURE §7). A commit certificate proves that
/// validators holding more than 2/3 of voting power precommitted a block id —
/// the deterministic, instant-finality core of the Tendermint-style engine.
///
/// Votes are BLS-signed and bind the signer's validator index, so each signer's
/// message is distinct. That lets BLS core AggregateVerify (which requires
/// distinct messages, for rogue-key safety) collapse N votes into one
/// certificate signature without needing proof-of-possession.
namespace blockchainer::consensus {

using crypto::Hash;

struct Validator {
  Bytes bls_pubkey;          ///< 48-byte BLS public key
  std::uint64_t power = 0;   ///< voting power
};

/// An ordered validator set; a validator's index is its position here.
class ValidatorSet {
 public:
  explicit ValidatorSet(std::vector<Validator> validators);

  std::size_t size() const { return validators_.size(); }
  const Validator& at(std::uint32_t i) const { return validators_.at(i); }
  std::uint64_t total_power() const { return total_power_; }

  /// Byzantine quorum: the minimum power that strictly exceeds 2/3 of the total
  /// (`floor(2*total/3) + 1`). Two quorums always intersect (each > 1/2), which
  /// is what makes conflicting commits impossible — the safety foundation.
  std::uint64_t quorum() const;

  /// Canonical commitment to the set.
  Hash hash() const;

  Bytes encode() const;
  static ValidatorSet decode_all(codec::ByteView bytes);

 private:
  std::vector<Validator> validators_;
  std::uint64_t total_power_ = 0;
};

/// Canonical bytes a validator signs for a precommit. Binds chain/height/round/
/// block id AND the validator index (→ distinct message per signer).
Bytes vote_signing_bytes(std::uint64_t chain_id, std::uint64_t height,
                         std::uint32_t round, const Hash& block_id,
                         std::uint32_t validator_index);

/// One validator's precommit.
struct Vote {
  std::uint32_t validator_index = 0;
  Bytes signature;  ///< BLS over vote_signing_bytes(...)
};

/// >2/3 precommits for (height, round, block_id), aggregated.
struct CommitCertificate {
  std::uint64_t height = 0;
  std::uint32_t round = 0;
  Hash block_id{};
  std::vector<std::uint32_t> signers;  ///< validator indices, ascending, unique
  Bytes agg_signature;                 ///< BLS aggregate of the signers' votes

  Bytes encode() const;
  static CommitCertificate decode_all(codec::ByteView bytes);
};

struct CommitError {
  bool ok = true;
  std::string error;
  static CommitError success() { return {true, {}}; }
  static CommitError failure(std::string e) { return {false, std::move(e)}; }
};

/// Sign a precommit vote with a validator's BLS key.
Vote sign_vote(std::uint64_t chain_id, std::uint64_t height, std::uint32_t round,
               const Hash& block_id, std::uint32_t validator_index,
               const crypto::KeyPair& bls_key);

/// Build a commit certificate from collected votes for the same
/// (height, round, block_id). Invalid or duplicate votes are dropped; returns
/// nullopt if the valid signing power does not reach quorum.
std::optional<CommitCertificate> aggregate_commit(
    std::uint64_t chain_id, const ValidatorSet& vset, std::uint64_t height,
    std::uint32_t round, const Hash& block_id, const std::vector<Vote>& votes);

/// Verify a commit certificate: well-formed signer list, quorum power, and a
/// valid BLS aggregate signature. Never throws.
CommitError verify_commit(std::uint64_t chain_id, const ValidatorSet& vset,
                          const CommitCertificate& cert);

}  // namespace blockchainer::consensus
