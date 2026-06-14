#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/codec.hpp"

/// Inbound interoperability — a federated, attestation-based cross-chain bridge
/// (ARCHITECTURE §11). A set of bridge validators (the attestor set) BLS-sign
/// cross-chain packets; an endpoint mints/releases only after verifying a >2/3
/// aggregated attestation, with replay protection. This reuses the consensus
/// validator-set + BLS-aggregation machinery.
///
/// NOTE (docs/IMPROVEMENTS.md): this is the *federated* trust model. A fully
/// trustless light-client bridge (verify foreign consensus + a Merkle-Patricia
/// inclusion proof) is the deferred upgrade; it slots in behind the same
/// "verify before mint" seam.
namespace blockchainer::interop::bridge {

using crypto::Hash;
using Bytes = std::vector<std::uint8_t>;

enum class TransferKind : std::uint8_t {
  kMint = 0,     ///< lock on source → mint wrapped on dest
  kRelease = 1,  ///< burn wrapped on source → release escrow on dest
};

struct TransferPayload {
  TransferKind kind = TransferKind::kMint;
  Hash asset{};
  codec::u128 amount = 0;
  Hash recipient{};

  Bytes encode() const;
  static TransferPayload decode_all(codec::ByteView bytes);
};

/// An IBC-style cross-chain packet.
struct Packet {
  std::uint64_t source_chain = 0;
  std::uint64_t dest_chain = 0;
  std::uint64_t nonce = 0;
  Bytes payload;

  Bytes encode() const;
  static Packet decode_all(codec::ByteView bytes);
  Hash commitment() const;  ///< domain-separated hash of the canonical packet
};

// ── Attestation (BLS, index-bound like consensus votes) ──────────────────────

Bytes attestation_signing_bytes(const Hash& commitment, std::uint32_t attestor_index);

struct AttestationVote {
  std::uint32_t attestor_index = 0;
  Bytes signature;
};

struct Attestation {
  std::vector<std::uint32_t> signers;
  Bytes agg_signature;
};

AttestationVote sign_attestation(const Hash& commitment, std::uint32_t attestor_index,
                                 const crypto::KeyPair& bls_key);

std::optional<Attestation> aggregate_attestation(const consensus::ValidatorSet& attestors,
                                                 const Hash& commitment,
                                                 const std::vector<AttestationVote>& votes);

bool verify_attestation(const consensus::ValidatorSet& attestors, const Hash& commitment,
                        const Attestation& attestation);

// ── Endpoint ─────────────────────────────────────────────────────────────────

struct BridgeResult {
  bool ok = false;
  std::string error;
  static BridgeResult success() { return {true, {}}; }
  static BridgeResult failure(std::string e) { return {false, std::move(e)}; }
};

class BridgeEndpoint {
 public:
  BridgeEndpoint(std::uint64_t local_chain, consensus::ValidatorSet attestors)
      : local_chain_(local_chain), attestors_(std::move(attestors)) {}

  /// Source side: lock [amount] of [asset] for [recipient] on [dest_chain].
  /// Escrows locally and returns the packet to relay.
  Packet lock(std::uint64_t dest_chain, const Hash& asset, codec::u128 amount,
              const Hash& recipient);

  /// Source side: burn previously-minted wrapped [asset] to release it back on
  /// [dest_chain]. Returns a Release packet to relay.
  Packet burn(std::uint64_t dest_chain, const Hash& asset, codec::u128 amount,
              const Hash& recipient);

  /// Dest side: verify the attestation, check replay, then mint (kMint) or
  /// release escrow (kRelease). Leaves state unchanged on any failure.
  BridgeResult receive(const Packet& packet, const Attestation& attestation);

  codec::u128 minted_balance(const Hash& asset, const Hash& recipient) const;
  codec::u128 released_balance(const Hash& asset, const Hash& recipient) const;
  codec::u128 escrow(const Hash& asset) const;
  bool processed(std::uint64_t source_chain, std::uint64_t nonce) const;

 private:
  std::uint64_t local_chain_;
  consensus::ValidatorSet attestors_;
  std::uint64_t next_nonce_ = 0;
  std::set<std::pair<std::uint64_t, std::uint64_t>> processed_;
  std::map<std::pair<Hash, Hash>, codec::u128> minted_;
  std::map<std::pair<Hash, Hash>, codec::u128> released_;
  std::map<Hash, codec::u128> escrow_;
};

}  // namespace blockchainer::interop::bridge
