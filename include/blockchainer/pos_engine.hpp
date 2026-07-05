#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "blockchainer/codec.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/consensus_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/types.hpp"

/// A proof-of-stake reference engine (ARCHITECTURE §7): a third consensus family
/// behind the same [ConsensusEngine] seam as BFT and PoW. Each height has a
/// single, deterministic, stake-weighted **slot leader**; a block is justified by
/// that leader's signature over the block id. More stake → more slots; a slashed
/// (zero-power) validator is never a leader — reusing the existing stake +
/// slashing machinery.
///
/// **Decoupling.** Proposer selection is a [ProposerPolicy] value injected at
/// construction, so the selection rule (default: deterministic stake-weighted)
/// can be swapped for round-robin, a VRF, or an epoch schedule without touching
/// the engine — the enhancement seam. The justification (the leader's signature)
/// is a *different* type from the BFT certificate and the PoW nonce, yet all
/// three plug into the same `submit(block, justification)`.
///
/// Reference-grade: no finality gadget beyond the leader signature and linear
/// chain extension (no fork choice / reorg) — larger follow-ups.
namespace blockchainer::consensus {

/// Deterministic proposer selection: given the height and the (stake-weighted)
/// validator set, return the slot leader's index. Must be pure.
using ProposerPolicy = std::function<std::uint32_t(std::uint64_t height, const ValidatorSet&)>;

/// Default policy: a deterministic ticket derived from the height picks a
/// validator with probability proportional to its stake. Zero-power validators
/// are never selected.
std::uint32_t stake_weighted_proposer(std::uint64_t height, const ValidatorSet& vset);

/// The PoS payload in a block's consensus_data: which validator proposed it.
struct PosData {
  std::uint32_t proposer_index = 0;

  Bytes encode() const;
  static PosData decode_all(codec::ByteView bytes);
};

/// Canonical bytes a slot leader signs to justify a block (domain-separated from
/// BFT precommit votes).
Bytes pos_signing_bytes(std::uint64_t chain_id, std::uint64_t height, const Hash& block_id);

class PosEngine : public ConsensusEngine {
 public:
  /// [me] is this node's validator index; [key] its BLS key (used to seal blocks
  /// when it is the leader). [policy] defaults to stake-weighted selection.
  PosEngine(ValidatorSet vset, std::uint32_t me, crypto::KeyPair key,
            std::unique_ptr<Ledger> ledger, ProposerPolicy policy = stake_weighted_proposer)
      : vset_(std::move(vset)),
        me_(me),
        key_(std::move(key)),
        ledger_(std::move(ledger)),
        policy_(std::move(policy)) {}

  const Ledger& ledger() const override { return *ledger_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }

  /// The stake-weighted leader for [height].
  std::uint32_t proposer_for(std::uint64_t height) const { return policy_(height, vset_); }

  /// Assemble the next block. Throws unless THIS node is the leader for tip+1.
  /// Stamps consensus_data with our proposer index; call seal() for the
  /// justifying signature.
  Block propose(std::vector<Transaction> txs, std::uint64_t timestamp) override;

  /// The leader's signature over [block]'s id — the block's justification.
  Bytes seal(const Block& block) const;

  /// Verify the block was produced by the legitimate stake-weighted leader for
  /// its height and that [justification] is that leader's valid signature, then
  /// apply it.
  BlockError submit(const Block& block, codec::ByteView justification) override;

 private:
  ValidatorSet vset_;
  std::uint32_t me_;
  crypto::KeyPair key_;
  std::unique_ptr<Ledger> ledger_;
  ProposerPolicy policy_;
};

}  // namespace blockchainer::consensus
