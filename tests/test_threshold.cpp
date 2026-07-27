// Threshold custody: FROST t-of-n accounts where the secret key NEVER exists.
// The aggregate is a standard Ed25519 signature, so a threshold-signed transfer
// commits through the untouched SigAlgo::kEd25519 path — the chain can't even
// tell threshold custody was used.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/threshold.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using custody::ShareCard;
using custody::SigningCeremony;
using custody::ThresholdAccount;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
}  // namespace

int main() {
  init();

  std::printf("== create 2-of-3: consistent cards, distinct shares ==\n");
  std::vector<ShareCard> cards = ThresholdAccount::create(3, 2);
  CHECK(cards.size() == 3);
  for (const auto& c : cards) {
    CHECK(c.group_public_key.size() == 32 && c.secret_share.size() == 32);
    CHECK(c.group_public_key == cards[0].group_public_key);
    CHECK(c.threshold == 2 && c.total == 3);
    CHECK(c.all_public_shares.size() == 3);
  }
  CHECK(cards[0].secret_share != cards[1].secret_share);
  CHECK(cards[0].index == 1 && cards[2].index == 3);

  std::printf("== a 2-of-3 ceremony signs a transfer the Ledger commits (kEd25519 path) ==\n");
  const Hash group_addr = crypto::address_of(view(cards[0].group_public_key));
  Transaction tx;
  tx.version = 1;  // plain v1 envelope — threshold custody is invisible on-chain
  tx.chain_id = 7;
  Hash to{};
  to[0] = 0xBB;
  tx.body = AccountStateMachine::encode_transfer({0, to, 400});
  tx.author = cards[0].group_public_key;
  const Bytes msg = tx.signing_bytes();

  // Round 1: shares 1 and 3 participate (any 2 of the 3).
  auto [n1, c1] = SigningCeremony::commit(cards[0]);
  auto [n3, c3] = SigningCeremony::commit(cards[2]);
  SigningCeremony ceremony(cards[0].group_public_key, msg, {c3, c1});  // order-insensitive
  CHECK(ceremony.roster().size() == 2 && ceremony.roster()[0].id == 1);

  // Round 2 + aggregate.
  const Bytes s1 = ceremony.sign(cards[0], n1);
  const Bytes s3 = ceremony.sign(cards[2], n3);
  CHECK(s1.size() == 32 && s3.size() == 32);
  tx.signature = ceremony.aggregate({s1, s3});
  CHECK_MSG(tx.signature.size() == 64, "the aggregate is a standard Ed25519 signature");
  CHECK_MSG(verify_transaction_signature(tx), "verifies through the untouched kEd25519 path");

  {
    auto genesis = std::make_unique<AccountStateMachine>();
    genesis->credit_genesis(group_addr, 1000);
    Ledger ledger(ChainSpec{7, StateModel::kAccount}, std::move(genesis));
    Block b = ledger.build_block({tx}, /*timestamp=*/1);
    CHECK(ledger.process_block(b).ok);
    CHECK_MSG(ledger.tip().height == 1, "a threshold-signed transfer commits on-chain");
  }

  std::printf("== below threshold fails: one share alone cannot sign ==\n");
  {
    auto [n2, c2] = SigningCeremony::commit(cards[1]);
    SigningCeremony solo(cards[0].group_public_key, msg, {c2});
    const Bytes s2 = solo.sign(cards[1], n2);
    bool ok = true;
    try {
      const Bytes sig = solo.aggregate({s2});
      ok = crypto::ed25519_verify(view(msg), view(sig), view(cards[0].group_public_key));
    } catch (const std::exception&) {
      ok = false;
    }
    CHECK_MSG(!ok, "t-1 participants cannot produce a valid group signature");
  }

  std::printf("== a lying cosigner is IDENTIFIED, not just rejected ==\n");
  {
    auto [na, ca] = SigningCeremony::commit(cards[0]);
    auto [nb, cb] = SigningCeremony::commit(cards[1]);
    SigningCeremony cer(cards[0].group_public_key, msg, {ca, cb});
    const Bytes good = cer.sign(cards[0], na);
    Bytes bad = cer.sign(cards[1], nb);
    bad[5] ^= 0x01;  // participant 2 contributes a corrupted share

    CHECK_MSG(cer.verify_share(1, cards[0].public_share, good), "honest share attributed as honest");
    CHECK_MSG(!cer.verify_share(2, cards[1].public_share, bad), "the corrupt cosigner is named");
    // And the corrupted aggregate is of course invalid.
    bool valid = true;
    try {
      const Bytes sig = cer.aggregate({good, bad});
      valid = crypto::ed25519_verify(view(msg), view(sig), view(cards[0].group_public_key));
    } catch (const std::exception&) {
      valid = false;
    }
    CHECK(!valid);
  }

  std::printf("== nonces bind the ceremony: signing against a different roster fails ==\n");
  {
    auto [na, ca] = SigningCeremony::commit(cards[0]);
    auto [nb, cb] = SigningCeremony::commit(cards[1]);
    auto [nc, cc] = SigningCeremony::commit(cards[1]);  // a second, different round-1
    SigningCeremony right(cards[0].group_public_key, msg, {ca, cb});
    SigningCeremony wrong(cards[0].group_public_key, msg, {ca, cc});
    const Bytes s_right = right.sign(cards[0], na);
    const Bytes s_cross = wrong.sign(cards[1], nb);  // share 2 signed vs a roster it isn't in
    bool valid = true;
    try {
      const Bytes sig = right.aggregate({s_right, s_cross});
      valid = crypto::ed25519_verify(view(msg), view(sig), view(cards[0].group_public_key));
    } catch (const std::exception&) {
      valid = false;
    }
    CHECK_MSG(!valid, "a share produced against a different roster never aggregates validly");
  }

  std::printf("== ShareCard codec + sealed round-trip; tampered binding fails closed ==\n");
  {
    const Bytes enc = cards[1].encode();
    CHECK_MSG(ShareCard::decode(view(enc)) == cards[1], "codec round-trips");

    // Light Argon2id work factors for the test; 0,0 (SENSITIVE) is the default.
    const Bytes env = cards[1].seal("share-pass", /*ops=*/3, /*mem=*/1u << 16);
    ShareCard back = ShareCard::unseal(view(env), "share-pass");
    CHECK_MSG(back == cards[1], "sealed share survives the round-trip");

    bool threw = false;
    try {
      (void)ShareCard::unseal(view(env), "wrong-pass");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "wrong passphrase fails closed");

    Bytes moved = env;
    moved[0] ^= 0x01;  // flip the cleartext index prefix → AAD mismatch
    threw = false;
    try {
      (void)ShareCard::unseal(view(moved), "share-pass");
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "re-binding the envelope to another slot fails closed");
  }

  return bctest::summary();
}
