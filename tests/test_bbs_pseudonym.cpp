// BBS per-verifier pseudonyms + blind issuance (crypto seam) —
// draft-irtf-cfrg-bbs-per-verifier-linkability-02. The pseudonym seed is
// committed BLINDLY at issuance (the issuer never learns it), so a holder derives
// a deterministic per-context pseudonym: same credential + same context → the
// SAME pseudonym (Sybil-linkable within a scope), different context → an
// unlinkable one. This is the primitive CredentialGate Option B is built on.
#include <string>
#include <vector>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes msg(const std::string& s) { return Bytes(s.begin(), s.end()); }
}  // namespace

int main() {
  init();

  auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x2a)));
  Bytes header = msg("blockchainer.credential.v2");
  Bytes ph = msg("presentation-nonce");

  // Signer-set (issuer) attributes + committed (holder-hidden) attributes.
  std::vector<Bytes> signer{msg("tier=3"), msg("not_after=2100")};  // L = 2
  std::vector<Bytes> committed{msg("region=eu"), msg("dob=1990")};

  // ── Nym-seed derivation via hash_to_scalar (the canonical-scalar path) ──────
  // A stable per-holder seed derived from the member secret: same member → same
  // seed → same per-context pseudonym. hash_to_scalar guarantees a canonical
  // scalar, so no clamping workaround is needed.
  Bytes member_secret(32, 0x77);
  Bytes prover_nym = crypto::bbs_hash_to_scalar(view(member_secret), view(msg("nym-seed.v1")));
  CHECK(prover_nym.size() == 32);
  CHECK(prover_nym[0] < 0x74);  // canonical: below the top byte of r (0x73ed…)
  // Deterministic in (msg, dst); a different domain → a different seed.
  CHECK(crypto::bbs_hash_to_scalar(view(member_secret), view(msg("nym-seed.v1"))) == prover_nym);
  CHECK(crypto::bbs_hash_to_scalar(view(member_secret), view(msg("other-dst"))) != prover_nym);

  // ── Blind issuance of the pseudonym seed ───────────────────────────────────
  // The issuer blind-signs over the seed without ever seeing it.
  std::vector<Bytes> prover_nyms{prover_nym};
  auto commit = crypto::bbs_commit_with_nym(committed, prover_nyms);
  CHECK(commit.secret_prover_blind.size() == 32);

  Bytes entropy = crypto::bbs_random_scalar();  // issuer contributes fresh entropy
  CHECK(entropy.size() == 32 && crypto::bbs_random_scalar() != entropy);
  Bytes sig = crypto::bbs_blind_sign_with_nym(view(issuer.secret_key), view(issuer.public_key),
                                              view(commit.commitment), view(header), signer,
                                              view(entropy), /*length_nym_vector=*/1);
  CHECK(sig.size() == 80);

  auto nym_secrets = crypto::bbs_finalize_nym_secrets(prover_nyms, view(entropy));
  CHECK(nym_secrets.size() == 1 && nym_secrets[0].size() == 32);

  // ── Present for context A, disclosing all attributes ───────────────────────
  Bytes ctxA = msg("epoch5|gossip");
  auto pres = crypto::bbs_proof_gen_with_pseudonym(
      view(issuer.public_key), view(sig), view(header), view(ph), view(ctxA), signer, committed,
      view(commit.secret_prover_blind), nym_secrets,
      /*disclosed_signer=*/{0, 1}, /*disclosed_committed=*/{0, 1});
  CHECK(pres.pseudonym.size() == 48);

  // The proof's pseudonym equals the deterministic calculate_pseudonym.
  Bytes nymA = crypto::bbs_calculate_pseudonym(view(ctxA), nym_secrets);
  CHECK(nymA == pres.pseudonym);

  // Verify: combined disclosed indexes — committed j → j + L + 1 (L=2), so
  // {signer 0, signer 1, committed 0, committed 1} → {0, 1, 3, 4}.
  std::vector<Bytes> disclosed{signer[0], signer[1], committed[0], committed[1]};
  CHECK(crypto::bbs_proof_verify_with_pseudonym(view(issuer.public_key), view(pres.proof),
                                                view(header), view(ph), view(ctxA),
                                                view(pres.pseudonym), /*L=*/2,
                                                /*length_nym_vector=*/1, disclosed, {0, 1, 3, 4}));

  // A verifier at a DIFFERENT context cannot verify this proof.
  Bytes ctxB = msg("epoch6|gossip");
  CHECK(!crypto::bbs_proof_verify_with_pseudonym(view(issuer.public_key), view(pres.proof),
                                                 view(header), view(ph), view(ctxB),
                                                 view(pres.pseudonym), 2, 1, disclosed, {0, 1, 3, 4}));
  // A tampered pseudonym is rejected.
  Bytes bad_nym = pres.pseudonym;
  bad_nym[10] ^= 0x01;
  CHECK(!crypto::bbs_proof_verify_with_pseudonym(view(issuer.public_key), view(pres.proof),
                                                 view(header), view(ph), view(ctxA), view(bad_nym),
                                                 2, 1, disclosed, {0, 1, 3, 4}));

  // ── Unlinkability + Sybil-linkability, the whole point ─────────────────────
  Bytes nymB = crypto::bbs_calculate_pseudonym(view(ctxB), nym_secrets);
  CHECK(nymB != nymA);                                                    // cross-context: unlinkable
  CHECK(crypto::bbs_calculate_pseudonym(view(ctxA), nym_secrets) == nymA);  // same context: stable

  // A different member (fresh seed) yields a different pseudonym in ctxA →
  // one credential = one pseudonym per context (Sybil resistance).
  Bytes other_nym = crypto::bbs_hash_to_scalar(view(Bytes(32, 0x88)), view(msg("nym-seed.v1")));
  auto other_secrets = crypto::bbs_finalize_nym_secrets({other_nym}, view(entropy));
  CHECK(crypto::bbs_calculate_pseudonym(view(ctxA), other_secrets) != nymA);

  // ── Standalone blind issuance (no pseudonyms) ──────────────────────────────
  auto bc = crypto::bbs_blind_commit(committed);
  CHECK(bc.secret_prover_blind.size() == 32);
  Bytes bsig = crypto::bbs_blind_sign(view(issuer.secret_key), view(issuer.public_key),
                                      view(bc.commitment), view(header), signer);
  CHECK(bsig.size() == 80);
  CHECK(crypto::bbs_verify_blind_sign(view(issuer.public_key), view(bsig), view(header), signer,
                                      committed, view(bc.secret_prover_blind)));
  // Wrong committed message / wrong blind both fail closed.
  CHECK(!crypto::bbs_verify_blind_sign(view(issuer.public_key), view(bsig), view(header), signer,
                                       {msg("region=us")}, view(bc.secret_prover_blind)));
  Bytes bad_blind = bc.secret_prover_blind;
  bad_blind[0] ^= 0x01;
  CHECK(!crypto::bbs_verify_blind_sign(view(issuer.public_key), view(bsig), view(header), signer,
                                       committed, view(bad_blind)));

  return bctest::summary();
}
