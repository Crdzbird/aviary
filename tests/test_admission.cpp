// The anonymous-membership admission handshake (docs/design/CREDENTIAL_GATE.md
// §6): a verifier challenges a connecting peer with a fresh nonce; the peer
// answers with an unlinkable "gossip"-scope credential presentation; the verifier
// admits or refuses. This drives the pure exchange end to end — the transport
// (run_tcp_bft_node) just moves these wire bytes over a socket.
#include <string>

#include "aviary/admission.hpp"
#include "aviary/credential_gate.hpp"
#include "aviary/crypto.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
crypto::ByteView view(const Bytes& b) { return crypto::ByteView(b.data(), b.size()); }
Bytes bytes(const std::string& s) { return Bytes(s.begin(), s.end()); }
}  // namespace

int main() {
  init();

  // Epoch derivation is deterministic (§10.5): height / epoch_length; 0 disables.
  CHECK(consensus::membership_epoch(0, 100) == 0);
  CHECK(consensus::membership_epoch(250, 100) == 2);
  CHECK(consensus::membership_epoch(999999, 0) == 0);  // rotation disabled → epoch 0

  // random_bytes returns the requested length and is non-repeating (nonce use).
  CHECK(crypto::random_bytes(32).size() == 32);
  CHECK(crypto::random_bytes(32) != crypto::random_bytes(32));

  // Issuer + policy + a member credential (tier 2, min tier 1).
  auto issuer = crypto::bbs_keygen(view(Bytes(32, 0x33)));
  consensus::CredentialPolicy policy;
  policy.chain_id = 11;
  policy.issuer_public_key = issuer.public_key;
  policy.header = bytes("aviary.credential.v1");
  policy.min_tier = 1;
  consensus::CredentialGate gate(policy);

  Bytes member_secret(32, 0x44);
  auto cred = consensus::issue_credential(policy, view(issuer.secret_key), view(member_secret),
                                          /*tier=*/2, /*not_after=*/4102444800ULL,
                                          /*issuer_epoch=*/1);
  const std::uint64_t now = 1700000000ULL;
  const std::uint32_t epoch = consensus::membership_epoch(500, 100);  // = 5

  // ── Happy path: challenge → present → admit ────────────────────────────────
  auto ch = p2p::make_admission_challenge(epoch);
  CHECK(ch.nonce.size() == 32);
  Bytes reply = p2p::answer_admission_challenge(policy, cred, view(ch.wire));
  CHECK(p2p::admission_granted(gate, epoch, view(ch.nonce), view(reply), now));

  // ── Unlinkability: a second exchange yields a different presentation, still
  //    admits (each challenge is a fresh nonce → fresh, uncorrelatable proof) ──
  auto ch2 = p2p::make_admission_challenge(epoch);
  Bytes reply2 = p2p::answer_admission_challenge(policy, cred, view(ch2.wire));
  CHECK(reply != reply2);
  CHECK(p2p::admission_granted(gate, epoch, view(ch2.nonce), view(reply2), now));

  // ── Replay across connections fails: a presentation for challenge A does not
  //    admit under a different verifier's nonce (each connection binds its own) ─
  CHECK(!p2p::admission_granted(gate, epoch, view(ch2.nonce), view(reply), now));

  // ── Epoch binding: a reply built for one epoch does not admit under another ─
  auto che = p2p::make_admission_challenge(epoch + 1);
  Bytes replye = p2p::answer_admission_challenge(policy, cred, view(che.wire));
  CHECK(!p2p::admission_granted(gate, epoch, view(che.nonce), view(replye), now));

  // ── Wrong issuer: a credential from another issuer is refused ──────────────
  auto rogue = crypto::bbs_keygen(view(Bytes(32, 0x99)));
  auto forged = consensus::issue_credential(policy, view(rogue.secret_key), view(member_secret), 2,
                                            4102444800ULL, 1);
  auto chf = p2p::make_admission_challenge(epoch);
  Bytes replyf = p2p::answer_admission_challenge(policy, forged, view(chf.wire));
  CHECK(!p2p::admission_granted(gate, epoch, view(chf.nonce), view(replyf), now));

  // ── A garbage reply is a quiet refusal, never a throw ──────────────────────
  auto chg = p2p::make_admission_challenge(epoch);
  CHECK(!p2p::admission_granted(gate, epoch, view(chg.nonce), view(bytes("not a wire message")), now));

  return bctest::summary();
}
