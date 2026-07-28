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

// A canonical 32-byte scalar (< the BLS12-381 scalar-field order): cryptolib does
// NOT reduce nym / entropy scalars, so a value >= r breaks the round-trip. Zeroing
// the top byte guarantees the value is far below r; the low bytes distinguish it.
Bytes scalar(std::uint8_t tag) {
  Bytes s(32, 0);
  for (std::size_t i = 1; i < 32; ++i) s[i] = static_cast<std::uint8_t>(tag + i);
  return s;  // s[0] == 0 → < 2^248 << r
}
}  // namespace

int main() {
  init();

  auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x2a)));
  Bytes header = msg("blockchainer.credential.v2");
  Bytes ph = msg("presentation-nonce");

  // Signer-set (issuer) attributes + committed (holder-hidden) attributes.
  std::vector<Bytes> signer{msg("tier=3"), msg("not_after=2100")};  // L = 2
  std::vector<Bytes> committed{msg("region=eu"), msg("dob=1990")};

  // ── Blind issuance of a pseudonym seed ─────────────────────────────────────
  // Holder picks a pseudonym seed and blind-commits it: the issuer signs over it
  // without ever seeing it.
  Bytes prover_nym = scalar(0x11);
  std::vector<Bytes> prover_nyms{prover_nym};
  auto commit = crypto::bbs_commit_with_nym(committed, prover_nyms);
  CHECK(commit.secret_prover_blind.size() == 32);

  Bytes entropy = scalar(0x22);
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

  // A different credential (fresh seed) yields a different pseudonym in ctxA →
  // one credential = one pseudonym per context (Sybil resistance).
  Bytes other_nym = scalar(0x33);
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
