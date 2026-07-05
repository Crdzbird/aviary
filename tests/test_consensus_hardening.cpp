// S4 consensus hardening: equivocation evidence, validator-set binding, and the
// LastCommit self-certifying chain.
#include <optional>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/version.hpp"
#include "blockchainer/wire.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }

Hash filled(std::uint8_t b) {
  Hash h{};
  for (auto& x : h) x = b;
  return h;
}

constexpr std::uint64_t kChain = 7;

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce,
                     codec::u128 amt) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}
}  // namespace

int main() {
  init();

  auto v0 = bls(10), v1 = bls(11), v2 = bls(12), v3 = bls(13);
  ValidatorSet vset(std::vector<Validator>{
      {v0.public_key, 1}, {v1.public_key, 1}, {v2.public_key, 1}, {v3.public_key, 1}});

  const Hash A = filled(0xAA), B = filled(0xBB);

  std::printf("== equivocation: a validator that double-signs is caught ==\n");
  {
    // Validator 2 precommits BOTH A and B at the same (height 5, round 0).
    Vote v2a = sign_vote(kChain, 5, 0, A, 2, v2);
    Vote v2b = sign_vote(kChain, 5, 0, B, 2, v2);
    std::vector<AttributedVote> votes = {
        {A, sign_vote(kChain, 5, 0, A, 0, v0)},  // honest
        {A, v2a},
        {B, v2b},  // conflict with v2a
    };
    auto ev = detect_equivocation(5, 0, votes);
    CHECK(ev.has_value());
    CHECK(verify_equivocation(kChain, vset, *ev).ok);

    // Round-trips through encode/decode.
    Evidence rt = Evidence::decode_all(view(ev->encode()));
    CHECK(verify_equivocation(kChain, vset, rt).ok);

    std::printf("== honest single votes yield no evidence ==\n");
    CHECK(!detect_equivocation(5, 0, {{A, sign_vote(kChain, 5, 0, A, 0, v0)}}).has_value());
    // Same validator, SAME block twice is not equivocation (idempotent vote).
    CHECK(!detect_equivocation(5, 0, {{A, v2a}, {A, sign_vote(kChain, 5, 0, A, 2, v2)}})
               .has_value());

    std::printf("== forged / malformed evidence is rejected ==\n");
    Evidence tampered = *ev;
    tampered.signature_b[0] ^= 0x01;
    CHECK(!verify_equivocation(kChain, vset, tampered).ok);  // bad signature
    Evidence oor = *ev;
    oor.validator_index = 99;
    CHECK(!verify_equivocation(kChain, vset, oor).ok);  // out of range
    Evidence same = *ev;
    same.block_id_b = same.block_id_a;
    CHECK(!verify_equivocation(kChain, vset, same).ok);  // not conflicting
    // Evidence that mislabels the signer (claims v1 signed v2's votes) fails.
    Evidence wrong_signer = *ev;
    wrong_signer.validator_index = 1;
    CHECK(!verify_equivocation(kChain, vset, wrong_signer).ok);
  }

  std::printf("== validator set rejects malformed construction ==\n");
  {
    // Empty set.
    CHECK(::bctest::throws([&] { ValidatorSet(std::vector<Validator>{}); }));
    // Wrong-length BLS key.
    CHECK(::bctest::throws(
        [&] { ValidatorSet(std::vector<Validator>{{Bytes(32, 0x01), 1}}); }));
    // Duplicate public keys.
    CHECK(::bctest::throws([&] {
      ValidatorSet(std::vector<Validator>{{v0.public_key, 1}, {v0.public_key, 1}});
    }));
    // No live power at all.
    CHECK(::bctest::throws(
        [&] { ValidatorSet(std::vector<Validator>{{v0.public_key, 0}}); }));
    // A single slashed validator among live ones is fine (zero power allowed).
    CHECK(!::bctest::throws([&] { (void)vset.with_slashed(0); }));
  }

  std::printf("== slashing zeroes the equivocator's power ==\n");
  {
    ValidatorSet slashed = vset.with_slashed(2);
    CHECK(slashed.total_power() == vset.total_power() - 1);
    CHECK(slashed.at(2).power == 0);
    CHECK(slashed.quorum() == (3 * 2) / 3 + 1);  // 3 remaining → quorum 3
  }

  std::printf("== evidence gossips over the wire and drives an engine evidence pool ==\n");
  {
    // Validator 1 double-signs; build the proof.
    Vote a = sign_vote(kChain, 9, 0, A, 1, v1);
    Vote b = sign_vote(kChain, 9, 0, B, 1, v1);
    auto ev = detect_equivocation(9, 0, {{A, a}, {B, b}});
    CHECK(ev.has_value());

    // Round-trips as a wire message.
    const Bytes wire_bytes = wire::make(wire::EvidenceMsg{*ev}).encode();
    wire::Message back =
        wire::Message::decode_all(codec::ByteView(wire_bytes.data(), wire_bytes.size()));
    wire::EvidenceMsg em = wire::as_evidence(back);
    CHECK(verify_equivocation(kChain, vset, em.evidence).ok);

    // An engine records it, exposes the offender, and can hand back a slashed set.
    auto g = std::make_unique<AccountStateMachine>();
    auto led = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
    BftEngine engine(vset, std::move(led));
    CHECK(engine.record_evidence(em.evidence));
    CHECK(engine.record_evidence(em.evidence));  // idempotent
    CHECK(engine.equivocators().count(1) == 1);
    ValidatorSet slashed = engine.slashed_validator_set();
    CHECK(slashed.at(1).power == 0);
    CHECK(slashed.total_power() == vset.total_power() - 1);

    // Invalid evidence (tampered signature) is neither accepted nor recorded.
    Evidence forged = em.evidence;
    forged.signature_a[0] ^= 0x01;
    CHECK(!engine.record_evidence(forged));
    CHECK(engine.equivocators().size() == 1);  // unchanged
  }

  // ── Validator-set binding + LastCommit, exercised through a real engine ──────
  auto vkey = bls(100);
  ValidatorSet single(std::vector<Validator>{{vkey.public_key, 1}});  // quorum 1
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  auto genesis = std::make_unique<AccountStateMachine>();
  genesis->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(genesis));
  BftEngine engine(single, std::move(ledger));
  const Hash genesis_id = engine.tip().hash();

  auto produce = [&](std::uint64_t nonce, codec::u128 amt, std::uint64_t ts) {
    Block b = engine.propose({transfer(alice, bob_addr, nonce, amt)}, ts);
    auto cert = engine.try_commit(b, {engine.precommit(b, 0, vkey)});
    CHECK(cert.has_value());
    CHECK_MSG(engine.finalize(b, *cert).ok, "block finalized");
    return b;
  };

  std::printf("== every produced block binds to the validator set ==\n");
  Block b1 = produce(0, 100, 1);
  CHECK(validate_consensus_data(kChain, single, b1.header, genesis_id).ok);
  // A follower using a DIFFERENT validator set rejects it (anti vset-swap).
  ValidatorSet impostor(std::vector<Validator>{{bls(200).public_key, 1}});
  CHECK(!validate_consensus_data(kChain, impostor, b1.header, genesis_id).ok);

  std::printf("== first block carries no last commit; later blocks certify the parent ==\n");
  Block b2 = produce(1, 50, 2);
  CHECK(validate_consensus_data(kChain, single, b2.header, b1.header.hash()).ok);
  // Wrong parent → the embedded last commit does not certify it.
  CHECK(!validate_consensus_data(kChain, single, b2.header, filled(0x11)).ok);

  std::printf("== tampered consensus_data is rejected ==\n");
  {
    // A height-1 header must not carry a last commit.
    BlockHeader as_first = b2.header;
    as_first.height = 1;
    CHECK(!validate_consensus_data(kChain, single, as_first, genesis_id).ok);

    // A height-2 header with the last commit stripped is rejected.
    BlockHeader stripped = b2.header;
    stripped.consensus_data = ConsensusData{single.hash(), std::nullopt}.encode();
    CHECK(!validate_consensus_data(kChain, single, stripped, b1.header.hash()).ok);
  }

  return bctest::summary();
}
