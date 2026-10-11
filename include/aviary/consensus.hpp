#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// BFT consensus primitives (ARCHITECTURE section 7). A commit certificate proves that
/// validators holding more than 2/3 of voting power precommitted a block id - 
/// the deterministic, instant-finality core of the Tendermint-style engine.
///
/// Votes are BLS-signed and bind the signer's validator index, so each signer's
/// message is distinct. That lets BLS core AggregateVerify (which requires
/// distinct messages, for rogue-key safety) collapse N votes into one
/// certificate signature without needing proof-of-possession.
namespace aviary::consensus {

using crypto::Hash;

struct Validator {
  Bytes bls_pubkey;          ///< 48-byte BLS public key
  std::uint64_t power = 0;   ///< voting power
  bool operator==(const Validator&) const = default;
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
  /// is what makes conflicting commits impossible - the safety foundation.
  std::uint64_t quorum() const;

  /// Canonical commitment to the set.
  Hash hash() const;

  /// A copy with validator [index]'s voting power zeroed - the punishment for
  /// proven equivocation (it can no longer contribute to any quorum). Indices
  /// are preserved so existing certificates still resolve. Applying it is a
  /// protocol/governance action (a validator-set change at some height); this
  /// just produces the slashed set.
  ValidatorSet with_slashed(std::uint32_t index) const;

  Bytes encode() const;
  static ValidatorSet decode_all(codec::ByteView bytes);
  const std::vector<Validator>& validators() const { return validators_; }
  /// The index of [bls_pubkey] in this set, or nullopt if it is not a member.
  std::optional<std::uint32_t> index_of(const Bytes& bls_pubkey) const;

 private:
  std::vector<Validator> validators_;
  std::uint64_t total_power_ = 0;
};

/// Canonical bytes a validator signs for a precommit. Binds chain/height/round/
/// block id AND the validator index (-> distinct message per signer).
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

// -- Consensus header payload (S4) -------------------------------------------
//
// What the BFT engine stores in BlockHeader::consensus_data (opaque to the
// ledger, but committed by the header hash - so validators sign it). It binds
// the block to a specific validator set and carries the previous block's commit
// certificate, so a stored/relayed chain self-certifies: each block proves its
// parent was finalized by >2/3, not merely that headers link.
struct ConsensusData {
  Hash validator_set_hash{};
  std::optional<CommitCertificate> last_commit;  ///< absent only for the first block
  /// A VALIDATOR-SET CHANGE: the set in force from the NEXT height on. The
  /// block announcing it is certified by the CURRENT set (>2/3 of the old set
  /// attests the new one), so the change is itself finalized; the following
  /// block's last_commit is verified under the old set and its own certificate
  /// under the new one. Absent = no change.
  std::optional<std::vector<Validator>> next_validators;

  Bytes encode() const;
  static ConsensusData decode_all(codec::ByteView bytes);
};

/// Validate a header's consensus_data against [vset] and its parent: the
/// committed validator-set hash must match (catches a validator-set swap), and
/// the embedded last commit must be a valid certificate for [parent_hash] at
/// height-1 (the first block after genesis carries none). Never throws.
CommitError validate_consensus_data(std::uint64_t chain_id, const ValidatorSet& vset,
                                    const BlockHeader& h, const Hash& parent_hash);

/// The rotation-aware form: [current] is the set in force at h.height (its hash
/// must be committed by the header and it certifies h itself); [certifying] is
/// the set that finalized the PARENT (it verifies the embedded last commit) - 
/// they differ exactly for the block after a validator-set change. Also checks
/// that an announced next set is well-formed. Never throws.
CommitError validate_consensus_data(std::uint64_t chain_id, const ValidatorSet& current,
                                    const ValidatorSet& certifying, const BlockHeader& h,
                                    const Hash& parent_hash);

/// The validator set a header announces for the next height, or nullopt when it
/// announces none or its consensus data is malformed / the set is invalid.
std::optional<ValidatorSet> announced_validator_set(const BlockHeader& h);

// -- Equivocation evidence (S4) ----------------------------------------------
//
// Proof that a validator double-signed: two valid precommits at the same
// (height, round) for DIFFERENT block ids - a deliberate attempt to violate
// safety, and the canonical slashable offense.
struct Evidence {
  std::uint32_t validator_index = 0;
  std::uint64_t height = 0;
  std::uint32_t round = 0;
  Hash block_id_a{};
  Bytes signature_a;
  Hash block_id_b{};
  Bytes signature_b;

  Bytes encode() const;
  static Evidence decode_all(codec::ByteView bytes);
};

/// A precommit annotated with the block id it was cast for (what a node collects
/// while gossiping votes; the raw Vote does not carry the id it signed).
struct AttributedVote {
  Hash block_id{};
  Vote vote;
};

/// Verify [ev] genuinely proves equivocation: same in-range signer, same
/// (height, round), two DISTINCT block ids, and BOTH signatures valid under the
/// signer's key. Never throws.
CommitError verify_equivocation(std::uint64_t chain_id, const ValidatorSet& vset,
                                const Evidence& ev);

/// Scan precommits at one (height, round) for a validator that signed two
/// different block ids; returns the first such conflict as Evidence (its
/// signatures still to be confirmed with verify_equivocation), else nullopt.
std::optional<Evidence> detect_equivocation(std::uint64_t height, std::uint32_t round,
                                            const std::vector<AttributedVote>& votes);

}  // namespace aviary::consensus
