#pragma once

#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "aviary/codec.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

/// Threshold custody (ARCHITECTURE section : Wallet / key custody, hardened tier):
/// t-of-n accounts via FROST(Ed25519, SHA-512) - RFC 9591 threshold Schnorr.
///
/// The property that matters: **the account's secret key never exists** - not
/// at creation (the dealer splits a key that is immediately discarded), not at
/// signing (each participant contributes a share; only signature shares are
/// combined). Compromise of up to t-1 devices reveals nothing.
///
/// The aggregate is a **standard 64-byte Ed25519 signature** under the group
/// public key, so a threshold-signed transaction verifies through the existing
/// `SigAlgo::kEd25519` path - the chain cannot even tell threshold custody was
/// used, and no consensus or wire format changes.
///
/// Flow: `ThresholdAccount::create(n, t)` deals n [ShareCard]s (distribute one
/// per device; seal each at rest). To sign: every participating share runs
/// round 1 (`SigningCeremony::commit`), the roster of commitments is fixed,
/// each share runs round 2 (`sign`), and `aggregate` yields the signature.
/// A malformed contribution is *attributable*: `verify_share` says which
/// cosigner lied.
namespace aviary::custody {

using Bytes = crypto::Bytes;
using crypto::ByteView;

/// One participant's share of a threshold account. `encode()` includes the
/// secret share - it exists to feed [seal]; never persist it unsealed.
struct ShareCard {
  std::uint16_t index = 0;      ///< FROST identifier (1-based)
  std::uint16_t threshold = 0;  ///< t - cosigners needed
  std::uint16_t total = 0;      ///< n - shares dealt
  Bytes group_public_key;       ///< 32 B - the account's on-chain author key
  Bytes secret_share;           ///< 32 B scalar - THIS device's secret
  Bytes public_share;           ///< 32 B point - proves this share's honesty
  std::vector<Bytes> all_public_shares;  ///< n x 32 B - verify ANY cosigner

  bool operator==(const ShareCard&) const = default;

  /// Canonical (versioned) serialization. Contains the SECRET share.
  Bytes encode() const;
  /// Strict inverse; throws codec::CodecError on malformed input.
  static ShareCard decode(codec::ByteView bytes);

  /// Seal for rest via MolecularVault's passphrase mode (Argon2id -> cascade;
  /// ops/mem 0,0 = the SENSITIVE preset). The AAD binds the group key and the
  /// share index, so a sealed share can't be replayed into another group or
  /// another slot. Fails closed on open.
  Bytes seal(std::string_view passphrase, std::uint64_t ops = 0, std::size_t mem = 0) const;
  static ShareCard unseal(codec::ByteView envelope, std::string_view passphrase);
};

/// Trusted-dealer creation of a t-of-n threshold account.
class ThresholdAccount {
 public:
  /// Deal [n] shares, any [t] of which can sign. The dealt group secret is
  /// zeroised inside cryptolib before this returns. Throws on bad parameters.
  static std::vector<ShareCard> create(std::uint16_t n, std::uint16_t t);
};

/// One two-round FROST signing ceremony over a fixed message and roster.
/// Coordinator view: collect round-1 commitments, construct the ceremony, have
/// each participant `sign`, then `aggregate`. Deterministic given the roster - 
/// every participant must see the SAME roster (same ids, same commitments).
class SigningCeremony {
 public:
  /// A participant's round-1 secret nonces. Use once, then discard - nonce
  /// reuse across ceremonies forfeits the key (standard Schnorr rule).
  struct Nonces {
    Bytes hiding;   ///< 32 B secret scalar
    Bytes binding;  ///< 32 B secret scalar
  };
  /// A participant's public round-1 commitment (safe to broadcast).
  struct Commitment {
    std::uint16_t id = 0;
    Bytes hiding;   ///< 32 B point
    Bytes binding;  ///< 32 B point
  };

  /// Round 1: fresh nonces + commitment for [share]. Nonces stay with the
  /// participant; the commitment goes to the coordinator.
  static std::pair<Nonces, Commitment> commit(const ShareCard& share);

  /// Fix the ceremony: the message to sign and the full roster of round-1
  /// commitments (>= threshold of them). The roster is canonicalised (sorted
  /// by id); duplicate ids throw.
  SigningCeremony(Bytes group_public_key, Bytes message, std::vector<Commitment> roster);

  /// Round 2: [share]'s 32-byte signature share using its round-1 [nonces].
  Bytes sign(const ShareCard& share, const Nonces& nonces) const;

  /// Attribute misbehaviour: does [sig_share] from participant [id] (whose
  /// public share is [public_share]) verify against this ceremony? Never throws.
  bool verify_share(std::uint16_t id, const Bytes& public_share, const Bytes& sig_share) const;

  /// Combine signature shares (ordered like [roster]()) into a standard 64-byte
  /// Ed25519 signature under the group public key. Throws on failure.
  Bytes aggregate(const std::vector<Bytes>& sig_shares) const;

  /// The canonicalised roster (ascending id) - sig_shares align to this order.
  const std::vector<Commitment>& roster() const { return roster_; }

 private:
  Bytes gpk_;
  Bytes msg_;
  std::vector<Commitment> roster_;   // ascending id
  std::vector<std::uint16_t> ids_;   // parallel arrays for the C ABI
  Bytes hiding_flat_;                // count x 32
  Bytes binding_flat_;               // count x 32
};

}  // namespace aviary::custody
