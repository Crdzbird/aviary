#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aviary/block_store.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/codec.hpp"
#include "aviary/genesis.hpp"
#include "aviary/types.hpp"

/// Chain persistence (docs/design/SUBCHAINS.md section 22, docs/CODEC_SPEC.md
/// "Chain snapshot"). A ChainSnapshot is everything an assembled chain needs to
/// resume after a restart and then produce the same bytes an uninterrupted run
/// would: the ledger (tip, state, subchain registry), the receipts of the tip
/// block, the engine's consensus state and, optionally, the archive of
/// finalized blocks with their certificates. It is bound to the chain's
/// genesis hash and holds no secret key: the caller supplies the validator key
/// again on restore. Canonical, versioned, strictly decoded.
namespace aviary::node {

struct ChainSnapshot {
  /// The format version this build writes and the only one it reads.
  static constexpr std::uint8_t kVersion = 1;

  Hash genesis_hash{};
  Bytes ledger;                    ///< Ledger::snapshot()
  std::vector<Receipt> receipts;   ///< Ledger::last_receipts()
  Bytes engine;                    ///< ConsensusEngine::consensus_state()
  std::optional<Bytes> archive;    ///< BlockStore::serialize(), when the chain keeps one

  /// "AVCS" || u8 version || genesis_hash || bytes(ledger) || list(bytes(receipt)) ||
  /// bytes(engine) || bool has_archive || [bytes(archive)].
  Bytes encode() const;
  /// Strict: a wrong magic, an unknown version, a malformed field or a
  /// trailing byte throws codec::CodecError.
  static ChainSnapshot decode_all(codec::ByteView bytes);
};

/// A chain brought back from a snapshot.
struct RestoredChain {
  AssembledChain chain;
  std::optional<BlockStore> archive;
};

/// Capture [chain], assembled from the genesis whose hash is [genesis_hash],
/// with [archive] if it keeps one.
Bytes snapshot_chain(const Hash& genesis_hash, const AssembledChain& chain, const BlockStore* archive);

/// Rebuild the chain [snapshot] was taken from, as validator [me] with
/// [bls_key]. Throws (std::exception) and builds nothing when the snapshot
/// does not decode, belongs to another genesis, is not byte for byte what the
/// rebuilt chain would write (non canonical), disagrees with its own tip (state
/// root, receipts, subchain registry, consensus state, certificate), or
/// carries an archive that is not a contiguous run of this chain's blocks
/// ending at the tip, or that a chain other than BFT carries. Archived blocks
/// are authenticated by hash linkage to the tip, whose certificate is
/// verified. Every archived certificate is verified as well, under the
/// validator set its block's header commits: the genesis set, the engine's
/// certifying and in force sets, or a set an archived header announces. A
/// certificate whose set is none of those (announced below the archive's
/// first height and since rotated out) must instead equal the last commit the
/// next archived header embeds; otherwise the snapshot is refused.
RestoredChain restore_chain(const GenesisSpec& genesis, std::uint32_t me, crypto::KeyPair bls_key,
                            codec::ByteView snapshot, Bytes vrf_secret = {},
                            std::vector<Bytes> vrf_pubkeys = {});

}  // namespace aviary::node
