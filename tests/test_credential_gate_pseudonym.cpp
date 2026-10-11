// CredentialGate Option B - pseudonymous anonymous membership
// (docs/design/CREDENTIAL_GATE.md section 3.2, section 5, section 6). A member blind-issues a
// credential whose pseudonym seed the issuer never sees, then admits itself with
// a per-(epoch, scope) pseudonym: same member + same epoch + scope -> the SAME
// pseudonym (Sybil-resistant + targeted revocation), a different epoch -> an
// unlinkable one.
#include <optional>
#include <string>
#include <vector>

#include "aviary/credential_gate.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using consensus::CredentialGate;
using consensus::CredentialMode;
using consensus::CredentialPolicy;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
Bytes challenge(std::uint8_t s) { return Bytes(32, s); }

// Full blind issuance: member_secret -> the 3-step protocol -> a credential.
consensus::PseudonymousCredential issue(const CredentialPolicy& policy, crypto::ByteView issuer_sk,
                                        crypto::ByteView member_secret, std::uint8_t tier,
                                        std::uint64_t not_after, std::uint32_t issuer_epoch) {
  auto mc = consensus::membership_commit(policy, member_secret);
  auto bi = consensus::blind_issue(policy, issuer_sk, view(mc.commitment), tier, not_after,
                                   issuer_epoch);
  return consensus::finalize_membership(mc, bi);
}
}  // namespace

int main() {
  init();

  auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x2a)));
  CredentialPolicy policy;
  policy.chain_id = 7;
  policy.issuer_public_key = issuer.public_key;
  policy.header = bytes("aviary.credential.v2");
  policy.min_tier = 2;
  policy.mode = CredentialMode::kPseudonymous;

  // The mode is part of chain identity: it round-trips through the policy codec.
  CHECK(CredentialPolicy::decode_all(view(policy.encode())).mode == CredentialMode::kPseudonymous);
  CHECK(CredentialPolicy::decode_all(view(policy.encode())) == policy);

  Bytes member_secret(32, 0x77);
  const std::uint64_t notAfter = 4102444800ULL;  // year 2100
  const std::uint64_t now = 1700000000ULL;
  auto cred = issue(policy, view(issuer.secret_key), view(member_secret), /*tier=*/3, notAfter, 1);
  CHECK(cred.signature.size() == 80);
  CHECK(cred.nym_secrets.size() == 1);

  CredentialGate gate(policy);

  // -- Happy path: present -> admit returns the pseudonym ----------------------
  Bytes pres = CredentialGate::present_pseudonymous(policy, cred, 5, "gossip", view(challenge(1)));
  auto nymA = gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(pres), now);
  CHECK(nymA.has_value());
  CHECK(nymA->size() == 48);

  // -- Sybil-linkability: a DIFFERENT presentation (fresh challenge) of the same
  //    member in the same (epoch, scope) yields the SAME pseudonym -------------
  Bytes pres2 = CredentialGate::present_pseudonymous(policy, cred, 5, "gossip", view(challenge(2)));
  CHECK(pres != pres2);  // the proofs differ on the wire (fresh randomness)
  auto nym2 = gate.admit_pseudonymous(5, "gossip", view(challenge(2)), view(pres2), now);
  CHECK(nym2.has_value() && *nym2 == *nymA);

  // Sybil cap: the first session on a nym is fresh; a second is flagged.
  CHECK(gate.note_admission(5, "gossip", view(*nymA)));   // first -> true
  CHECK(!gate.note_admission(5, "gossip", view(*nym2)));  // duplicate -> false

  // -- Unlinkable across epochs / scopes --------------------------------------
  Bytes presE = CredentialGate::present_pseudonymous(policy, cred, 6, "gossip", view(challenge(1)));
  auto nymE = gate.admit_pseudonymous(6, "gossip", view(challenge(1)), view(presE), now);
  CHECK(nymE.has_value() && *nymE != *nymA);  // different epoch -> different, uncorrelatable nym
  Bytes presS = CredentialGate::present_pseudonymous(policy, cred, 5, "submit", view(challenge(1)));
  auto nymS = gate.admit_pseudonymous(5, "submit", view(challenge(1)), view(presS), now);
  CHECK(nymS.has_value() && *nymS != *nymA);  // different scope -> different nym

  // A different member -> a different pseudonym in the same context (Sybil basis).
  auto other = issue(policy, view(issuer.secret_key), view(Bytes(32, 0x88)), 3, notAfter, 1);
  Bytes presO = CredentialGate::present_pseudonymous(policy, other, 5, "gossip", view(challenge(1)));
  auto nymO = gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(presO), now);
  CHECK(nymO.has_value() && *nymO != *nymA);

  // -- Context binding (anti-replay) ------------------------------------------
  CHECK(!gate.admit_pseudonymous(6, "gossip", view(challenge(1)), view(pres), now));    // wrong epoch
  CHECK(!gate.admit_pseudonymous(5, "submit", view(challenge(1)), view(pres), now));    // wrong scope
  CHECK(!gate.admit_pseudonymous(5, "gossip", view(challenge(9)), view(pres), now));    // wrong challenge

  // -- Policy checks: expiry, tier --------------------------------------------
  auto expired = issue(policy, view(issuer.secret_key), view(member_secret), 3, 1000ULL, 1);
  Bytes pe = CredentialGate::present_pseudonymous(policy, expired, 5, "gossip", view(challenge(1)));
  CHECK(!gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(pe), now));
  auto low = issue(policy, view(issuer.secret_key), view(member_secret), /*tier=*/1, notAfter, 1);
  Bytes pl = CredentialGate::present_pseudonymous(policy, low, 5, "gossip", view(challenge(1)));
  CHECK(!gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(pl), now));

  // -- Targeted nym revocation (section 6.2) -----------------------------------------
  {
    CredentialGate revoking(policy);
    revoking.revoke_nym(view(*nymA));
    Bytes pr = CredentialGate::present_pseudonymous(policy, cred, 5, "gossip", view(challenge(3)));
    CHECK(!revoking.admit_pseudonymous(5, "gossip", view(challenge(3)), view(pr), now));  // nym banned
    // A different epoch's nym for the same member is NOT banned (the nym changed).
    Bytes pr6 = CredentialGate::present_pseudonymous(policy, cred, 6, "gossip", view(challenge(3)));
    CHECK(revoking.admit_pseudonymous(6, "gossip", view(challenge(3)), view(pr6), now).has_value());
  }

  // -- Bulk issuer-epoch revocation (section 6.1) ------------------------------------
  {
    CredentialGate revoking(policy);
    revoking.revoke_issuer_epoch(1);
    Bytes pr = CredentialGate::present_pseudonymous(policy, cred, 5, "gossip", view(challenge(4)));
    CHECK(!revoking.admit_pseudonymous(5, "gossip", view(challenge(4)), view(pr), now));
  }

  // -- Forged issuer + malformed presentation fail closed ---------------------
  auto rogue = crypto::bbs_keygen(view(Bytes(32, 0x99)));
  auto forged = issue(policy, view(rogue.secret_key), view(member_secret), 3, notAfter, 1);
  Bytes pf = CredentialGate::present_pseudonymous(policy, forged, 5, "gossip", view(challenge(1)));
  CHECK(!gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(pf), now));
  CHECK(!gate.admit_pseudonymous(5, "gossip", view(challenge(1)), view(bytes("garbage")), now));

  return bctest::summary();
}
