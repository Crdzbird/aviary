#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aviary/crypto.hpp"
#include "aviary/types.hpp"

namespace aviary {
struct GenesisSpec;
}  // namespace aviary

/// Anonymous membership admission for a permissioned chain (design:
/// docs/design/CREDENTIAL_GATE.md). An issuer signs a member's attribute vector
/// as a BBS credential; a member then admits itself to a role (submit / gossip)
/// by proving, in zero knowledge, that it holds an issuer-signed credential
/// whose `tier` clears a policy threshold and whose expiry has not passed —
/// revealing nothing else (not its identity, not any other attribute).
///
/// Two modes (CREDENTIAL_GATE.md §3), selected by [CredentialPolicy::mode]:
///
/// **Option A — unlinkable** (§3.1, §12.2): two presentations from the same
/// credential are cryptographically unlinkable. Maximal privacy, but only *weak*
/// Sybil resistance (one credential can back many "distinct" presentations).
/// Functions: `issue_credential` / `CredentialGate::present` / `admit`.
///
/// **Option B — pseudonymous** (§3.2): each presentation carries a deterministic
/// per-(epoch, scope) pseudonym derived from a holder seed the issuer never sees
/// (blind BBS issuance). Same member + same epoch + scope → the SAME pseudonym
/// (real Sybil resistance + targeted per-epoch revocation), a different epoch →
/// an unlinkable one. The seed is `bbs_hash_to_scalar(member_secret)`. Functions:
/// `membership_commit` → `blind_issue` → `finalize_membership` (the 3-step blind
/// protocol), then `present_pseudonymous` / `admit_pseudonymous` (which returns
/// the pseudonym for Sybil-cap / nym-revocation bookkeeping).
namespace aviary::consensus {

/// The fixed attribute layout the issuer signs (CREDENTIAL_GATE.md §4). The
/// index of each well-known attribute is part of chain identity: a verifier must
/// agree on what each signed message means.
namespace credential_attr {
inline constexpr std::uint64_t kMemberSecret = 0;  ///< 32 B, never disclosed (Option B seed)
inline constexpr std::uint64_t kTier = 1;          ///< u8, disclosed on demand for the policy
inline constexpr std::uint64_t kNotAfter = 2;      ///< u64 unix, always disclosed (expiry)
inline constexpr std::uint64_t kIssuerEpoch = 3;   ///< u32, always disclosed (issuance round)
inline constexpr std::uint32_t kBaseCount = 4;     ///< the four well-known attributes
}  // namespace credential_attr

/// The membership/pseudonym epoch a given block height falls in (§10.5).
/// [epoch_length] is heights-per-epoch; 0 disables rotation (a single epoch 0).
/// Deterministic — every node derives the same epoch from the same height, so a
/// presentation's epoch binding is unambiguous.
inline std::uint32_t membership_epoch(std::uint64_t height, std::uint32_t epoch_length) {
  return epoch_length == 0 ? 0u : static_cast<std::uint32_t>(height / epoch_length);
}

/// The membership policy a chain enforces. Its [hash] is designed to be committed
/// into the genesis (like [AssuranceProfile]) so every node provably agrees on
/// the issuer, schema, and threshold — see CREDENTIAL_GATE.md §7. (Wiring it into
/// GenesisSpec is deferred pending the §10.4 schema decision; the policy stands
/// alone and hashable until then.)
/// Which admission mode a chain uses (committed in the policy, hence in genesis).
enum class CredentialMode : std::uint8_t {
  kUnlinkable = 0,    ///< Option A — bare BBS, unlinkable presentations
  kPseudonymous = 1,  ///< Option B — per-(epoch, scope) pseudonym, Sybil-resistant
};

struct CredentialPolicy {
  std::uint64_t chain_id = 0;         ///< the chain this policy governs
  Bytes issuer_public_key;            ///< 96-byte BBS public key of the credential issuer
  Bytes header;                       ///< BBS issuance-domain header (a fixed domain string)
  std::uint32_t attribute_count = credential_attr::kBaseCount;  ///< total signed attributes (>= 4)
  std::uint8_t min_tier = 0;          ///< admit iff disclosed tier >= min_tier
  CredentialMode mode = CredentialMode::kUnlinkable;  ///< which admission mode (§3)

  /// Canonical encoding (docs/CODEC_SPEC.md conventions).
  Bytes encode() const;
  /// Strict canonical decode (rejects trailing bytes / malformed input).
  static CredentialPolicy decode_all(crypto::ByteView bytes);
  /// Domain-separated commitment: hash_tagged(kCredentialPolicy, encode()).
  crypto::Hash hash() const;

  /// Stamp this policy into [g] so it becomes part of the chain's identity (its
  /// genesis hash) — the permissioned analog of AssuranceProfile::apply_to.
  /// Orthogonal to the assurance posture (§10.4).
  void commit_to(GenesisSpec& g) const;

  /// The membership policy a genesis commits to, or std::nullopt if the chain is
  /// permissionless (no policy). Throws if the committed bytes are malformed.
  static std::optional<CredentialPolicy> from_genesis(const GenesisSpec& g);

  bool operator==(const CredentialPolicy&) const = default;
};

/// A holder's credential: the issuer's 80-byte BBS signature over the full
/// attribute vector [attrs]. [attrs] is holder-secret (it contains member_secret
/// and any undisclosed attributes) and must be stored sealed at rest.
struct Credential {
  Bytes signature;
  std::vector<Bytes> attrs;
};

/// Issuer side (trusted-issuer model, CREDENTIAL_GATE.md §5/§10.2): assemble and
/// sign a credential. `member_secret` is 32 bytes generated by the *member* and
/// passed to the issuer over an authenticated channel; blind issuance (so the
/// issuer never sees it) awaits the cryptolib BBS blind-commit primitive.
/// `extra` are any policy-specific attributes beyond the four well-known ones;
/// they are signed and hidden by default. Throws if inputs are inconsistent with
/// [policy] or on backend failure.
Credential issue_credential(const CredentialPolicy& policy, crypto::ByteView issuer_secret_key,
                            crypto::ByteView member_secret, std::uint8_t tier,
                            std::uint64_t not_after, std::uint32_t issuer_epoch,
                            const std::vector<Bytes>& extra = {});

// ── Option B — pseudonymous, blind issuance (§3.2, §5) ───────────────────────

/// Holder side, step 1: derive a stable nym seed from [member_secret]
/// (`bbs_hash_to_scalar`, so the same member always gets the same seed) and
/// blind-commit it. Send [commitment] to the issuer; keep [nym_seed] and
/// [secret_prover_blind] secret to finalize. Throws on backend failure.
struct MembershipCommitment {
  Bytes commitment;           ///< send to the issuer
  Bytes nym_seed;             ///< holder secret (a canonical scalar)
  Bytes secret_prover_blind;  ///< holder secret
};
MembershipCommitment membership_commit(const CredentialPolicy& policy,
                                       crypto::ByteView member_secret);

/// Issuer side, step 2: blind-sign the holder's [commitment] with the issuer-set
/// attributes, contributing fresh nym entropy (so neither party alone fixes the
/// pseudonym). The issuer never learns the seed. Throws on backend failure.
struct BlindIssuance {
  Bytes signature;             ///< 80-byte BBS signature
  Bytes entropy;              ///< the issuer's nym entropy (the holder needs it to finalize)
  std::uint8_t tier = 0;      ///< the issuer-set attributes, echoed for the holder
  std::uint64_t not_after = 0;
  std::uint32_t issuer_epoch = 0;
};
BlindIssuance blind_issue(const CredentialPolicy& policy, crypto::ByteView issuer_secret_key,
                          crypto::ByteView commitment, std::uint8_t tier, std::uint64_t not_after,
                          std::uint32_t issuer_epoch);

/// Holder side, step 3: finalize the nym secrets and assemble the credential.
/// Holder-secret — store sealed at rest. Throws on backend failure.
struct PseudonymousCredential {
  Bytes signature;
  std::uint8_t tier = 0;
  std::uint64_t not_after = 0;
  std::uint32_t issuer_epoch = 0;
  std::vector<Bytes> nym_secrets;   ///< holder secret
  Bytes secret_prover_blind;        ///< holder secret
};
PseudonymousCredential finalize_membership(const MembershipCommitment& commit,
                                           const BlindIssuance& issued);

/// The admission gate. Holder-side [present] and verifier-side [admit] both bind
/// the presentation to (chain_id, epoch, scope, challenge) so a proof made for
/// one context never verifies in another.
class CredentialGate {
 public:
  explicit CredentialGate(CredentialPolicy policy) : policy_(std::move(policy)) {}

  const CredentialPolicy& policy() const { return policy_; }

  /// Holder side: build an unlinkable presentation for [challenge] in [scope]
  /// ("submit" / "gossip"). Discloses {tier, not_after, issuer_epoch}; proves
  /// possession of the whole credential in zero knowledge of everything else.
  /// Throws on backend failure or a credential inconsistent with [policy].
  static Bytes present(const CredentialPolicy& policy, const Credential& cred,
                       std::uint32_t epoch, std::string_view scope, crypto::ByteView challenge);

  /// Verifier side: admit? Runs the Option-A checklist (CREDENTIAL_GATE.md §6):
  ///   1. the BBS proof verifies under the issuer key + context-bound header,
  ///   2. not_after > now (expiry / coarse revocation),
  ///   3. issuer_epoch is not revoked (bulk revocation, §6.1),
  ///   4. disclosed tier >= policy.min_tier.
  /// Returns true iff admitted. Never throws — any malformed presentation,
  /// failed proof, or unmet policy is a quiet false. Unlinkable: keeps no
  /// per-identity state (that is Option B).
  bool admit(std::uint32_t epoch, std::string_view scope, crypto::ByteView challenge,
             crypto::ByteView presentation, std::uint64_t now) const;

  // ── Option B — pseudonymous (§3.2, §6) ─────────────────────────────────────

  /// Holder side: build a pseudonymous presentation for [challenge] in [scope].
  /// Discloses {tier, not_after, issuer_epoch}; carries a per-(chain, epoch,
  /// scope) pseudonym stable across this member's presentations in that context.
  /// Throws on backend failure.
  static Bytes present_pseudonymous(const CredentialPolicy& policy,
                                    const PseudonymousCredential& cred, std::uint32_t epoch,
                                    std::string_view scope, crypto::ByteView challenge);

  /// Verifier side: admit? Runs the §6 checklist (proof + expiry + issuer-epoch
  /// revocation + tier + nym not revoked). On success returns the disclosed
  /// **pseudonym** (for Sybil-cap / revocation bookkeeping); std::nullopt on any
  /// reject. Never throws. Pure — enforce the Sybil cap on the returned nym via
  /// [note_admission].
  std::optional<Bytes> admit_pseudonymous(std::uint32_t epoch, std::string_view scope,
                                          crypto::ByteView challenge, crypto::ByteView presentation,
                                          std::uint64_t now) const;

  /// Targeted revocation (§6.2): refuse this exact pseudonym. Because a nym is
  /// stable only within an epoch+scope, a persistent ban must be re-applied as
  /// the offender's nym changes each epoch.
  void revoke_nym(crypto::ByteView nym) { revoked_nyms_.emplace(nym.begin(), nym.end()); }
  bool nym_revoked(crypto::ByteView nym) const {
    return revoked_nyms_.count(Bytes(nym.begin(), nym.end())) != 0;
  }

  /// Sybil cap (§6.3): record an admitted [nym] for ([epoch], [scope]) and report
  /// whether this is the FIRST session on it — a false means a second distinct
  /// session is claiming an already-admitted pseudonym (the caller rate-limits or
  /// refuses). Mutating; call once per accepted session.
  bool note_admission(std::uint32_t epoch, std::string_view scope, crypto::ByteView nym);

  /// Bulk revocation: refuse every credential from [epoch] (§6.1).
  void revoke_issuer_epoch(std::uint32_t epoch) { revoked_epochs_.insert(epoch); }
  bool issuer_epoch_revoked(std::uint32_t epoch) const { return revoked_epochs_.count(epoch) != 0; }

 private:
  CredentialPolicy policy_;
  std::set<std::uint32_t> revoked_epochs_;
  std::set<Bytes> revoked_nyms_;
  std::set<std::pair<std::pair<std::uint32_t, std::string>, Bytes>> admitted_;  // (epoch,scope)→nym
};

}  // namespace aviary::consensus
