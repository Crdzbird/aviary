#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/consensus_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/types.hpp"

/// VRF-based proof-of-stake (ARCHITECTURE §7, hardened tier): a composed engine
/// that closes the reference [PosEngine]'s grinding gap. Its stake-weighted
/// leader is a *public deterministic function of the height* — predictable
/// arbitrarily far ahead (targeted DoS) and, with any influence over the seed,
/// grindable. Here, eligibility is an **ECVRF proof** (RFC 9381):
///
///   alpha = tag ‖ chain_id ‖ height ‖ attempt ‖ parent_hash
///   pi    = ecvrf_prove(vrf_sk, alpha)         (only the leader can make it)
///   beta  = proof_to_hash(pi)                  (fixed — nothing to grind)
///   eligible ⇔ (beta as ticket) mod total_power < oversample × power
///
/// Nobody can predict a future leader (beta needs the leader's secret key), the
/// leader can't shop for a favourable outcome (beta is a function, not a
/// choice), and everyone can verify eligibility from the proof carried in the
/// block. `parent_hash` in alpha pins the proof to this exact chain tip. If no
/// validator is eligible at an attempt, all retry with attempt+1 (bounded).
///
/// The VRF public keys are engine configuration (index-aligned with the
/// validator set), mirroring the transport's identity allow-list; a production
/// deployment would commit them in the genesis spec.
namespace blockchainer::consensus {

/// The VRF-PoS payload in a block's consensus_data (its own versioned type —
/// the reference PosEngine's PosData format is untouched).
struct VrfPosData {
  std::uint32_t proposer_index = 0;
  std::uint32_t attempt = 0;  ///< which retry round proved eligibility
  Bytes vrf_proof;            ///< 80-byte ECVRF proof over alpha(height, attempt, parent)

  Bytes encode() const;
  static VrfPosData decode_all(codec::ByteView bytes);
};

class VrfPosEngine : public ConsensusEngine {
 public:
  /// Liveness/uniqueness trade: expected eligible leaders per attempt is about
  /// [oversample]; retries cover an empty attempt, first valid block wins a
  /// crowded one (the linear ledger arbitrates; a fork-choice would).
  static constexpr std::uint32_t kDefaultOversample = 3;
  static constexpr std::uint32_t kMaxAttempts = 16;

  /// [vrf_pubkeys] is index-aligned with [vset]. [vrf_secret] is THIS
  /// validator's VRF seed; [key] its BLS key (seals blocks it leads).
  VrfPosEngine(ValidatorSet vset, std::vector<Bytes> vrf_pubkeys, std::uint32_t me,
               crypto::KeyPair key, Bytes vrf_secret, std::unique_ptr<Ledger> ledger,
               std::uint32_t oversample = kDefaultOversample);

  const Ledger& ledger() const override { return *ledger_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }

  /// Is THIS validator an eligible leader for tip+1 at [attempt]? Returns the
  /// ECVRF proof if so (deterministic: same tip → same answer).
  std::optional<Bytes> eligible(std::uint32_t attempt) const;

  /// Assemble the next block: finds the first attempt (0..kMaxAttempts) at
  /// which this validator is eligible, and stamps consensus_data with the
  /// proof. Throws if not eligible at any attempt.
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) override;
  Block propose_anchored(std::vector<Transaction> txs, std::uint64_t timestamp,
                         std::vector<Anchor> anchors) override;

  /// The leader's BLS signature over the block id — the block's justification
  /// (same sealing rule as the reference PoS engine).
  Bytes seal(const Block& block) const;

  /// Verify the embedded VRF proof (right key, right alpha, threshold met,
  /// attempt in range) AND the leader's signature, then apply the block.
  BlockError submit(const Block& block, codec::ByteView justification) override;

 private:
  /// The VRF input for deciding [height] at [attempt] on top of [parent].
  Bytes alpha(std::uint64_t height, std::uint32_t attempt, const Hash& parent) const;
  /// The eligibility rule on a VRF output for validator [index].
  bool meets_threshold(const Bytes& beta, std::uint32_t index) const;

  ValidatorSet vset_;
  std::vector<Bytes> vrf_pubkeys_;
  std::uint32_t me_;
  crypto::KeyPair key_;
  Bytes vrf_secret_;
  std::unique_ptr<Ledger> ledger_;
  std::uint32_t oversample_;
};

}  // namespace blockchainer::consensus
