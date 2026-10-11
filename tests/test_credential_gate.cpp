// CredentialGate - Option A (unlinkable anonymous membership admission).
// docs/design/CREDENTIAL_GATE.md section 3.1/section 6/section 12. An issuer signs a member's
// attribute vector as a BBS credential; the member admits itself to a role by
// proving in zero knowledge that it holds an issuer-signed credential whose tier
// clears the policy and whose expiry has not passed - revealing nothing else.
#include <string>
#include <vector>

#include "aviary/credential_gate.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using consensus::Credential;
using consensus::CredentialGate;
using consensus::CredentialPolicy;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
Bytes challenge(std::uint8_t s) { return Bytes(32, s); }
}  // namespace

int main() {
  init();

  // -- Issuer + policy --------------------------------------------------------
  Bytes ikm(32, 0x11);
  auto issuer = crypto::bbs_keygen(view(ikm));
  CredentialPolicy policy;
  policy.chain_id = 7;
  policy.issuer_public_key = issuer.public_key;
  policy.header = bytes("aviary.credential.v1");
  policy.attribute_count = consensus::credential_attr::kBaseCount;  // just the four well-known
  policy.min_tier = 2;

  // The policy is canonically encodable + hashable (ready to commit into genesis).
  CHECK(CredentialPolicy::decode_all(view(policy.encode())) == policy);
  CHECK(policy.hash() != crypto::Hash{});

  // -- Issue a credential: tier 3, far-future expiry, issuer epoch 1 ----------
  Bytes member_secret(32, 0xAB);
  Credential cred = consensus::issue_credential(policy, view(issuer.secret_key), view(member_secret),
                                                /*tier=*/3, /*not_after=*/4102444800ULL,
                                                /*issuer_epoch=*/1);
  CHECK(cred.signature.size() == 80);
  CHECK(cred.attrs.size() == 4);

  CredentialGate gate(policy);
  const std::uint64_t now = 1700000000ULL;  // well before not_after

  // -- Happy path: present for "submit" scope, admit --------------------------
  Bytes ch = challenge(0x01);
  Bytes pres = CredentialGate::present(policy, cred, /*epoch=*/5, "submit", view(ch));
  CHECK(gate.admit(5, "submit", view(ch), view(pres), now));

  // -- Unlinkability: a fresh presentation of the SAME credential differs on the
  //    wire (zero-knowledge randomization) yet still admits --------------------
  Bytes pres2 = CredentialGate::present(policy, cred, 5, "submit", view(ch));
  CHECK(pres != pres2);
  CHECK(gate.admit(5, "submit", view(ch), view(pres2), now));

  // -- Context binding (anti-replay): a proof made for one (epoch/scope/challenge)
  //    does not verify against another -----------------------------------------
  CHECK(!gate.admit(6, "submit", view(ch), view(pres), now));           // wrong epoch
  CHECK(!gate.admit(5, "gossip", view(ch), view(pres), now));           // wrong scope
  CHECK(!gate.admit(5, "submit", view(challenge(0x02)), view(pres), now));  // wrong challenge

  // -- Expiry: a credential past not_after is refused -------------------------
  Credential expired = consensus::issue_credential(policy, view(issuer.secret_key),
                                                   view(member_secret), 3,
                                                   /*not_after=*/1000ULL, 1);
  Bytes ep = CredentialGate::present(policy, expired, 5, "submit", view(ch));
  CHECK(!gate.admit(5, "submit", view(ch), view(ep), now));

  // -- Tier policy: a credential below min_tier is refused --------------------
  Credential low = consensus::issue_credential(policy, view(issuer.secret_key), view(member_secret),
                                               /*tier=*/1, 4102444800ULL, 1);
  Bytes lp = CredentialGate::present(policy, low, 5, "submit", view(ch));
  CHECK(!gate.admit(5, "submit", view(ch), view(lp), now));
  // tier == min_tier is admitted (boundary).
  Credential atmin = consensus::issue_credential(policy, view(issuer.secret_key),
                                                view(member_secret), /*tier=*/2, 4102444800ULL, 1);
  Bytes ap = CredentialGate::present(policy, atmin, 5, "submit", view(ch));
  CHECK(gate.admit(5, "submit", view(ch), view(ap), now));

  // -- Bulk revocation: revoke issuer epoch 1 -> the (tier-3) credential is out -
  {
    CredentialGate revoking(policy);
    revoking.revoke_issuer_epoch(1);
    Bytes rp = CredentialGate::present(policy, cred, 5, "submit", view(ch));
    CHECK(!revoking.admit(5, "submit", view(ch), view(rp), now));
  }

  // -- Wrong issuer: a credential from another issuer is refused --------------
  auto rogue = crypto::bbs_keygen(view(Bytes(32, 0x77)));
  Credential forged = consensus::issue_credential(policy, view(rogue.secret_key),
                                                 view(member_secret), 3, 4102444800ULL, 1);
  Bytes fp = CredentialGate::present(policy, forged, 5, "submit", view(ch));
  CHECK(!gate.admit(5, "submit", view(ch), view(fp), now));

  // -- Malformed presentation is a quiet reject, never a throw ----------------
  CHECK(!gate.admit(5, "submit", view(ch), view(bytes("garbage")), now));
  CHECK(!gate.admit(5, "submit", view(ch), crypto::ByteView{}, now));

  return bctest::summary();
}
