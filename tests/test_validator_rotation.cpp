// Validator-set changes (rotation): a block announces the next set; it is
// certified by the CURRENT set; from the next height the new set is in force.
// Proves the BFT engine, the light client and the vote-time governance rule.
#include <cstdio>
#include <memory>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/light_client.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls_key(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
constexpr std::uint64_t kChain = 9;
std::unique_ptr<Ledger> fresh_ledger() {
  return std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::make_unique<AccountStateMachine>());
}
// All of [signers] (index, key) precommit → certificate.
std::optional<CommitCertificate> certify(const BftEngine& e, const Block& b,
                                         const std::vector<std::pair<std::uint32_t, crypto::KeyPair>>& signers) {
  std::vector<Vote> votes;
  for (const auto& [i, k] : signers) votes.push_back(e.precommit(b, i, k));
  return e.try_commit(b, votes);
}
}  // namespace

int main() {
  init();
  std::vector<crypto::KeyPair> k = {bls_key(10), bls_key(11), bls_key(12), bls_key(13), bls_key(14)};
  std::vector<Validator> old_vs = {{k[0].public_key, 1}, {k[1].public_key, 1}, {k[2].public_key, 1}, {k[3].public_key, 1}};
  std::vector<Validator> new_vs = {{k[1].public_key, 1}, {k[2].public_key, 1}, {k[3].public_key, 1}, {k[4].public_key, 1}};  // drop 0, add 4 → indices shift
  ValidatorSet old_set(old_vs), new_set(new_vs);
  CHECK(old_set.hash() != new_set.hash());
  CHECK(old_set.index_of(k[3].public_key) == 3u && new_set.index_of(k[3].public_key) == 2u && !new_set.index_of(k[0].public_key));

  std::printf("== consensus data round-trips an announced set; validity is checked ==\n");
  {
    ConsensusData cd{old_set.hash(), std::nullopt, new_vs};
    ConsensusData back = ConsensusData::decode_all(view(cd.encode()));
    CHECK(back.next_validators && ValidatorSet(*back.next_validators).hash() == new_set.hash());
    ConsensusData none{old_set.hash(), std::nullopt};
    CHECK(!ConsensusData::decode_all(view(none.encode())).next_validators);
  }

  BftEngine engine(old_set, fresh_ledger());     // the scheduler
  BftEngine follower(old_set, fresh_ledger());   // a validator that did NOT schedule
  const BlockHeader genesis = engine.tip();

  std::printf("== block 1 under the old set ==\n");
  Block b1 = engine.propose({}, 1);
  auto c1 = certify(engine, b1, {{0, k[0]}, {1, k[1]}, {2, k[2]}});
  CHECK(c1 && engine.finalize(b1, *c1).ok && follower.finalize(b1, *c1).ok);
  CHECK(engine.validators().hash() == old_set.hash() && engine.tip_validators().hash() == old_set.hash());

  std::printf("== block 2 announces the rotation; certified by the OLD set ==\n");
  engine.schedule_validator_set(new_set);
  Block b2 = engine.propose({}, 2);
  CHECK(announced_validator_set(b2.header) && announced_validator_set(b2.header)->hash() == new_set.hash());
  CHECK_THROWS(follower.precommit(b2, 1, k[1]));                       // governance: unscheduled → refuses to vote
  auto c2 = certify(engine, b2, {{0, k[0]}, {1, k[1]}, {2, k[2]}});   // old indices, old keys
  CHECK(c2.has_value());
  CHECK(engine.finalize(b2, *c2).ok);
  CHECK(engine.validators().hash() == new_set.hash());                // in force for the next block
  CHECK(engine.tip_validators().hash() == old_set.hash());            // the tip was certified by the old set
  CHECK(!engine.scheduled_validator_set());
  CHECK(follower.finalize(b2, *c2).ok);                               // certified → the follower FOLLOWS
  CHECK(follower.validators().hash() == new_set.hash());

  std::printf("== block 3: hash of the NEW set, last commit verified under the OLD set ==\n");
  Block b3 = engine.propose({}, 3);
  {
    ConsensusData cd = ConsensusData::decode_all(view(b3.header.consensus_data));
    CHECK(cd.validator_set_hash == new_set.hash() && cd.last_commit && cd.last_commit->height == 2 && !cd.next_validators);
  }
  // Votes from old-set members under their OLD indices no longer verify (keys moved / left).
  CHECK(!certify(engine, b3, {{0, k[0]}, {1, k[1]}, {2, k[2]}}));
  auto c3 = certify(engine, b3, {{0, k[1]}, {1, k[2]}, {2, k[3]}});   // new indices
  CHECK(c3 && engine.finalize(b3, *c3).ok && engine.height() == 3);
  CHECK(engine.tip_validators().hash() == new_set.hash());
  // The newcomer can vote too: a quorum built with validator 4.
  Block b4 = engine.propose({}, 4);
  auto c4 = certify(engine, b4, {{1, k[2]}, {2, k[3]}, {3, k[4]}});
  CHECK(c4 && engine.finalize(b4, *c4).ok);

  std::printf("== a block certified by the old set after the rotation is refused ==\n");
  {
    BftEngine stale(old_set, fresh_ledger());
    CHECK(stale.finalize(b1, *c1).ok && stale.finalize(b2, *c2).ok);   // stale now holds the new set
    // Forge b3 as if the old set still ruled: old hash, votes by old members.
    Block forged = b3;
    ConsensusData cd = ConsensusData::decode_all(view(forged.header.consensus_data));
    cd.validator_set_hash = old_set.hash();
    forged.header.consensus_data = cd.encode();
    BftEngine oldworld(old_set, fresh_ledger());
    CHECK(oldworld.finalize(b1, *c1).ok);
    // (oldworld cannot even build a matching cert for forged: b2's rotation was certified.)
    std::vector<Vote> votes;
    for (std::uint32_t i = 0; i < 3; ++i) votes.push_back(sign_vote(kChain, 3, 0, forged.header.hash(), i, k[i]));
    auto fc = aggregate_commit(kChain, old_set, 3, 0, forged.header.hash(), votes);
    CHECK(fc.has_value());
    CHECK(!stale.finalize(forged, *fc).ok);                            // hash mismatch under the new set
  }

  std::printf("== a malformed announced set is refused before it can take effect ==\n");
  {
    BftEngine e2(old_set, fresh_ledger());
    Block bad = e2.propose({}, 1);
    ConsensusData cd = ConsensusData::decode_all(view(bad.header.consensus_data));
    cd.next_validators = std::vector<Validator>{};                     // empty set
    bad.header.consensus_data = cd.encode();
    std::vector<Vote> votes;
    for (std::uint32_t i = 0; i < 3; ++i) votes.push_back(sign_vote(kChain, 1, 0, bad.header.hash(), i, k[i]));
    auto bc = aggregate_commit(kChain, old_set, 1, 0, bad.header.hash(), votes);
    CHECK(bc && !e2.finalize(bad, *bc).ok && e2.height() == 0);
    CHECK(!announced_validator_set(bad.header));
    cd.next_validators = std::vector<Validator>{{Bytes(47, 1), 1}};   // bad key length
    bad.header.consensus_data = cd.encode();
    CHECK(!announced_validator_set(bad.header));
  }

  std::printf("== light client follows the rotation; one that ignores it cannot continue ==\n");
  {
    client::LightClient lc(kChain, old_set, genesis);
    CHECK(lc.accept(b1.header, *c1).ok);
    CHECK(lc.accept(b2.header, *c2).ok);
    CHECK(lc.validators().hash() == new_set.hash() && lc.tip_validators().hash() == old_set.hash());
    CHECK(lc.accept(b3.header, *c3).ok);
    CHECK(lc.tip_validators().hash() == new_set.hash());
    CHECK(lc.accept(b4.header, *c4).ok && lc.height() == 4);
    // A client resumed at b2 but still holding the OLD set for the next header:
    client::LightClient ignorant(kChain, old_set, b2.header);
    CHECK(!ignorant.accept(b3.header, *c3).ok);
    // Resumed correctly across the boundary (new set next, old set certified the tip):
    client::LightClient resumed(kChain, new_set, b2.header, old_set);
    CHECK(resumed.accept(b3.header, *c3).ok && resumed.accept(b4.header, *c4).ok);
    // Resumed with the wrong certifying set: b3's last commit (old-set votes) fails.
    client::LightClient wrong(kChain, new_set, b2.header, new_set);
    CHECK(!wrong.accept(b3.header, *c3).ok);
  }

  std::printf("== a node that leaves the set keeps following ==\n");
  {
    BftEngine left(old_set, fresh_ledger());                          // "validator 0", dropped by the rotation
    left.schedule_validator_set(new_set);
    CHECK(left.finalize(b1, *c1).ok && left.finalize(b2, *c2).ok && left.finalize(b3, *c3).ok && left.finalize(b4, *c4).ok);
    CHECK(left.height() == 4 && !left.validators().index_of(k[0].public_key));
  }

  return bctest::summary();
}
