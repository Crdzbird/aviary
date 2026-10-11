// VRF-based PoS: leader eligibility is an ECVRF proof - unpredictable until the
// leader reveals it, un-grindable (the output is a function of (sk, alpha), not
// a choice), and verifiable by everyone from the proof in the block.
#include <memory>
#include <optional>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/ledger.hpp"
#include "aviary/version.hpp"
#include "aviary/vrf_pos.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }

// VRF keys from FIXED seeds, never ecvrf_keygen(): a VRF proof is deterministic
// in (sk, alpha), so fixed seeds make every eligibility outcome in this test
// reproducible run to run - the determinism mandate applies to tests too.
crypto::KeyPair vrf_key(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::KeyPair{crypto::ecvrf_public_key(view(seed)), seed};
}

std::unique_ptr<Ledger> genesis_ledger(const Hash& alice) {
  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice, 1000);
  return std::make_unique<Ledger>(ChainSpec{7, StateModel::kAccount}, std::move(g));
}
}  // namespace

int main() {
  init();
  const Hash alice = crypto::address_of(view(ed(1).public_key));

  // 4 validators with unequal stake, each with a fixed-seed VRF key -> the whole
  // test (which validator is eligible at which attempt) is reproducible.
  const std::uint32_t n = 4;
  std::vector<Validator> vs;
  std::vector<crypto::KeyPair> vrf;
  const std::uint64_t powers[n] = {40, 30, 20, 10};
  for (std::uint32_t i = 0; i < n; ++i) {
    vs.push_back(Validator{bls(static_cast<std::uint8_t>(0x60 + i)).public_key, powers[i]});
    vrf.push_back(vrf_key(static_cast<std::uint8_t>(0xA0 + i)));
  }
  const ValidatorSet vset(vs);
  std::vector<Bytes> vrf_pks;
  for (const auto& k : vrf) vrf_pks.push_back(k.public_key);

  auto make_engine = [&](std::uint32_t i) {
    return std::make_unique<VrfPosEngine>(vset, vrf_pks, i, bls(static_cast<std::uint8_t>(0x60 + i)),
                                          vrf[i].secret_key, genesis_ledger(alice));
  };
  std::vector<std::unique_ptr<VrfPosEngine>> engines;
  for (std::uint32_t i = 0; i < n; ++i) engines.push_back(make_engine(i));

  std::printf("== a VRF leader emerges and every engine accepts its block ==\n");
  // Find the first (attempt, validator) that is eligible - what a live network
  // converges to as attempts tick.
  std::optional<std::uint32_t> leader;
  for (std::uint32_t attempt = 0; attempt < VrfPosEngine::kMaxAttempts && !leader; ++attempt) {
    for (std::uint32_t i = 0; i < n; ++i) {
      if (engines[i]->eligible(attempt)) {
        leader = i;
        break;
      }
    }
  }
  CHECK_MSG(leader.has_value(), "some validator is eligible within the attempt budget");

  Block b = engines[*leader]->propose({}, /*timestamp=*/1);
  const Bytes seal = engines[*leader]->seal(b);
  for (std::uint32_t i = 0; i < n; ++i) {
    CHECK_MSG(engines[i]->submit(b, view(seal)).ok, "every engine verifies the proof + seal");
    CHECK(engines[i]->tip().hash() == engines[0]->tip().hash());
  }
  CHECK(engines[0]->height() == 1);

  std::printf("== eligibility is deterministic per tip (re-query agrees) ==\n");
  {
    for (std::uint32_t i = 0; i < n; ++i) {
      const bool a = engines[i]->eligible(0).has_value();
      const bool b2 = engines[i]->eligible(0).has_value();
      CHECK(a == b2);
    }
  }

  std::printf("== chain extends across several heights (leaders vary with the tip) ==\n");
  {
    for (int h = 2; h <= 5; ++h) {
      std::optional<std::uint32_t> l;
      for (std::uint32_t attempt = 0; attempt < VrfPosEngine::kMaxAttempts && !l; ++attempt)
        for (std::uint32_t i = 0; i < n && !l; ++i)
          if (engines[i]->eligible(attempt)) l = i;
      CHECK(l.has_value());
      Block nb = engines[*l]->propose({}, static_cast<std::uint64_t>(h));
      const Bytes s = engines[*l]->seal(nb);
      for (std::uint32_t i = 0; i < n; ++i) CHECK(engines[i]->submit(nb, view(s)).ok);
    }
    CHECK(engines[0]->height() == 5);
    for (std::uint32_t i = 1; i < n; ++i)
      CHECK(engines[i]->tip().hash() == engines[0]->tip().hash());
  }

  std::printf("== adversarial: forged / stolen / off-tip proofs are rejected ==\n");
  {
    // Build an honest next block to mutate.
    std::optional<std::uint32_t> l;
    std::uint32_t la = 0;
    for (std::uint32_t attempt = 0; attempt < VrfPosEngine::kMaxAttempts && !l; ++attempt)
      for (std::uint32_t i = 0; i < n && !l; ++i)
        if (engines[i]->eligible(attempt)) { l = i; la = attempt; }
    CHECK(l.has_value());
    Block good = engines[*l]->propose({}, 99);
    const Bytes s = engines[*l]->seal(good);
    const VrfPosData gd = VrfPosData::decode_all(view(good.header.consensus_data));
    CHECK(gd.attempt == la);

    // (a) tampered proof bytes.
    Block bad = good;
    VrfPosData d = gd;
    d.vrf_proof[10] ^= 0x01;
    bad.header.consensus_data = d.encode();
    CHECK(!engines[0]->submit(bad, view(engines[*l]->seal(bad))).ok);

    // (b) impostor: claim another validator's index with the real leader's proof.
    Block imp = good;
    VrfPosData di = gd;
    di.proposer_index = (gd.proposer_index + 1) % n;
    imp.header.consensus_data = di.encode();
    CHECK(!engines[0]->submit(imp, view(engines[di.proposer_index]->seal(imp))).ok);

    // (c) attempt out of range.
    Block far = good;
    VrfPosData df = gd;
    df.attempt = VrfPosEngine::kMaxAttempts;
    far.header.consensus_data = df.encode();
    CHECK(!engines[0]->submit(far, view(engines[*l]->seal(far))).ok);

    // (d) proposer index out of range.
    Block oob = good;
    VrfPosData dr = gd;
    dr.proposer_index = n + 3;
    oob.header.consensus_data = dr.encode();
    CHECK(!engines[0]->submit(oob, view(s)).ok);

    // (e) a corrupted leader seal.
    Bytes bad_seal = s;
    bad_seal[4] ^= 0x01;
    CHECK(!engines[0]->submit(good, view(bad_seal)).ok);

    // (f) an ineligible validator's GENUINE proof (valid VRF, fails threshold).
    //     Scan (validator, attempt) pairs for one that is not eligible on this
    //     exact tip, then have it honestly prove and still be rejected.
    bool found_ineligible = false;
    for (std::uint32_t att = 0; att < VrfPosEngine::kMaxAttempts && !found_ineligible; ++att) {
      for (std::uint32_t i = 0; i < n && !found_ineligible; ++i) {
        if (engines[i]->eligible(att).has_value()) continue;
        const Bytes a = [&] {
          codec::CodecWriter w;
          w.write_u8(0x03);  // kVrfEligibility
          w.write_u64(7);    // chain_id
          w.write_u64(engines[i]->tip().height + 1);
          w.write_u32(att);
          const Hash tip = engines[i]->tip().hash();
          w.write_fixed_bytes(codec::ByteView(tip.data(), 32), 32);
          return w.take();
        }();
        Block cheat = good;
        VrfPosData dc;
        dc.proposer_index = i;
        dc.attempt = att;
        dc.vrf_proof = crypto::ecvrf_prove(view(vrf[i].secret_key), view(a));  // honest proof
        cheat.header.consensus_data = dc.encode();
        CHECK_MSG(!engines[0]->submit(cheat, view(engines[i]->seal(cheat))).ok,
                  "a genuine proof that fails the stake threshold is rejected");
        found_ineligible = true;
      }
    }
    CHECK_MSG(found_ineligible, "test exercised the threshold-rejection path");

    // The honest block still lands after all rejections.
    for (std::uint32_t i = 0; i < n; ++i) CHECK(engines[i]->submit(good, view(s)).ok);
  }

  std::printf("== stake weighting: over many tips, high stake leads more ==\n");
  {
    // Count round-0 eligibility per validator across many synthetic parents.
    // (Statistical smoke, wide margins: 40-power should beat 10-power clearly.)
    auto probe = make_engine(0);
    int wins[n] = {0, 0, 0, 0};
    // Drive the probe chain forward with whatever leader emerges, tallying who
    // is eligible at attempt 0 for each new tip.
    std::vector<std::unique_ptr<VrfPosEngine>> chain;
    for (std::uint32_t i = 0; i < n; ++i) chain.push_back(make_engine(i));
    for (int h = 1; h <= 60; ++h) {
      for (std::uint32_t i = 0; i < n; ++i)
        if (chain[i]->eligible(0)) ++wins[i];
      std::optional<std::uint32_t> l;
      for (std::uint32_t attempt = 0; attempt < VrfPosEngine::kMaxAttempts && !l; ++attempt)
        for (std::uint32_t i = 0; i < n && !l; ++i)
          if (chain[i]->eligible(attempt)) l = i;
      Block nb = chain[*l]->propose({}, static_cast<std::uint64_t>(1000 + h));
      const Bytes s = chain[*l]->seal(nb);
      for (std::uint32_t i = 0; i < n; ++i) CHECK(chain[i]->submit(nb, view(s)).ok);
    }
    CHECK_MSG(wins[0] > wins[3], "the 40-power validator is eligible more often than the 10-power one");
  }

  return bctest::summary();
}
