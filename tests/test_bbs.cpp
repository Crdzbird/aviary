// BBS signatures (crypto seam): multi-message signing + zero-knowledge selective
// disclosure. This is the primitive CredentialGate is built on — an issuer signs
// a vector of attributes, a holder later proves possession revealing only a
// chosen subset. See docs/design/CREDENTIAL_GATE.md.
#include <string>
#include <vector>

#include "blockchainer/crypto.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Bytes msg(const std::string& s) { return Bytes(s.begin(), s.end()); }
}  // namespace

int main() {
  init();

  // ── Issuer keypair ─────────────────────────────────────────────────────────
  Bytes ikm(32, 0x2a);
  auto issuer = crypto::bbs_keygen(view(ikm));
  CHECK(issuer.public_key.size() == 96);
  CHECK(issuer.secret_key.size() == 32);
  CHECK(crypto::bbs_sk_to_pk(view(issuer.secret_key)) == issuer.public_key);
  // Keygen is deterministic in the key material.
  CHECK(crypto::bbs_keygen(view(ikm)).public_key == issuer.public_key);

  // ── A membership credential: {member_id, tier, region, not_after} ──────────
  Bytes header = msg("blockchainer.credential.v1");
  std::vector<Bytes> attrs{msg("member-42"), msg("tier=3"), msg("region=eu"), msg("2030-01-01")};
  Bytes sig =
      crypto::bbs_sign(view(issuer.secret_key), view(issuer.public_key), view(header), attrs);
  CHECK(sig.size() == 80);
  CHECK(crypto::bbs_verify(view(issuer.public_key), view(sig), view(header), attrs));

  // Tamper: a flipped attribute must not verify.
  auto tampered = attrs;
  tampered[1] = msg("tier=9");
  CHECK(!crypto::bbs_verify(view(issuer.public_key), view(sig), view(header), tampered));
  // A different header (domain) rejects.
  CHECK(!crypto::bbs_verify(view(issuer.public_key), view(sig), view(msg("other")), attrs));
  // A different issuer's key rejects (issuer-unforgeability).
  auto other = crypto::bbs_keygen(view(Bytes(32, 0x99)));
  CHECK(!crypto::bbs_verify(view(other.public_key), view(sig), view(header), attrs));

  // ── Selective disclosure: reveal ONLY {tier, not_after} (indexes 1, 3) ─────
  Bytes ph = msg("challenge-nonce-abc");
  std::vector<std::uint64_t> disclosed_idx{1, 3};
  std::vector<Bytes> disclosed{attrs[1], attrs[3]};
  Bytes proof = crypto::bbs_proof_gen(view(issuer.public_key), view(sig), view(header), view(ph),
                                      attrs, disclosed_idx);
  CHECK(crypto::bbs_proof_verify(view(issuer.public_key), view(proof), view(header), view(ph),
                                 disclosed, disclosed_idx));

  // A verifier told the WRONG disclosed value fails.
  std::vector<Bytes> wrong_disclosed{msg("tier=9"), attrs[3]};
  CHECK(!crypto::bbs_proof_verify(view(issuer.public_key), view(proof), view(header), view(ph),
                                  wrong_disclosed, disclosed_idx));
  // The proof is bound to ph: a different challenge rejects (anti-replay).
  Bytes ph2 = msg("challenge-nonce-xyz");
  CHECK(!crypto::bbs_proof_verify(view(issuer.public_key), view(proof), view(header), view(ph2),
                                  disclosed, disclosed_idx));
  // A tampered proof fails.
  Bytes bad_proof = proof;
  bad_proof[7] ^= 0x01;
  CHECK(!crypto::bbs_proof_verify(view(issuer.public_key), view(bad_proof), view(header), view(ph),
                                  disclosed, disclosed_idx));

  // ── Full disclosure round-trips too ────────────────────────────────────────
  std::vector<std::uint64_t> all_idx{0, 1, 2, 3};
  Bytes full = crypto::bbs_proof_gen(view(issuer.public_key), view(sig), view(header), view(ph),
                                     attrs, all_idx);
  CHECK(crypto::bbs_proof_verify(view(issuer.public_key), view(full), view(header), view(ph), attrs,
                                 all_idx));

  // ── Disclose nothing (pure "I hold an issuer-signed credential") ───────────
  std::vector<std::uint64_t> none_idx{};
  std::vector<Bytes> none{};
  Bytes zk = crypto::bbs_proof_gen(view(issuer.public_key), view(sig), view(header), view(ph), attrs,
                                   none_idx);
  CHECK(crypto::bbs_proof_verify(view(issuer.public_key), view(zk), view(header), view(ph), none,
                                 none_idx));

  return bctest::summary();
}
