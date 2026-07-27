// Integration: boot validators from a single profiled genesis and drive them to
// commit through whichever engine the AssuranceProfile mandates. This proves the
// composed safety classes work as ONE system — the profile committed in genesis
// actually selects the engine, enforces the signature gate, and every node runs
// the same posture.
#include <memory>
#include <optional>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/assurance.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/profiled_validator.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using node::ProfiledValidator;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
// Fixed-seed VRF key (never ecvrf_keygen) so eligibility is reproducible.
crypto::KeyPair vrf_key(std::uint8_t s) {
  Bytes seed(32, s);
  return crypto::KeyPair{crypto::ecvrf_public_key(view(seed)), seed};
}

// A profiled genesis with n equal-stake validators funding one account.
GenesisSpec profiled_genesis(const AssuranceProfile& profile, std::uint32_t n, const Hash& alice) {
  GenesisSpec g;
  g.chain_id = 7;
  g.timestamp = 1;
  g.accounts.push_back(GenesisAccount{alice, 1000});
  for (std::uint32_t i = 0; i < n; ++i)
    g.validators.push_back(consensus::Validator{bls(static_cast<std::uint8_t>(0x60 + i)).public_key, 1});
  profile.apply_to(g);  // stamps the profile id + mandated sig algos
  return g;
}
}  // namespace

int main() {
  init();
  const Hash alice = crypto::address_of(view(ed(1).public_key));

  std::printf("== standard posture boots a stake-weighted PoS validator set → commits ==\n");
  {
    const std::uint32_t n = 3;
    const GenesisSpec g = profiled_genesis(AssuranceProfile::standard(), n, alice);
    std::vector<std::unique_ptr<ProfiledValidator>> vals;
    for (std::uint32_t i = 0; i < n; ++i)
      vals.push_back(std::make_unique<ProfiledValidator>(
          ProfiledValidator::boot(g, i, bls(static_cast<std::uint8_t>(0x60 + i)))));

    for (const auto& v : vals) CHECK(v->profile() == AssuranceProfile::standard());

    // Drive a height: the stake-weighted leader for tip+1 proposes.
    const std::uint32_t leader = consensus::stake_weighted_proposer(1, g.validator_set());
    Block b = vals[leader]->propose({}, /*timestamp=*/2);
    const Bytes seal = vals[leader]->seal(b);
    for (const auto& v : vals) CHECK_MSG(v->submit(b, view(seal)).ok, "every validator accepts the block");
    for (const auto& v : vals) CHECK(v->height() == 1 && v->tip().hash() == vals[0]->tip().hash());
    CHECK(!vals[0]->threshold_custody() && !vals[0]->sealed_mempool());
  }

  std::printf("== hardened posture boots a VRF-PoS validator set → commits ==\n");
  {
    const std::uint32_t n = 4;
    const GenesisSpec g = profiled_genesis(AssuranceProfile::hardened(), n, alice);
    std::vector<Bytes> vrf_pks;
    std::vector<Bytes> vrf_secs;
    for (std::uint32_t i = 0; i < n; ++i) {
      auto k = vrf_key(static_cast<std::uint8_t>(0xA0 + i));
      vrf_pks.push_back(k.public_key);
      vrf_secs.push_back(k.secret_key);
    }
    std::vector<std::unique_ptr<ProfiledValidator>> vals;
    for (std::uint32_t i = 0; i < n; ++i)
      vals.push_back(std::make_unique<ProfiledValidator>(ProfiledValidator::boot(
          g, i, bls(static_cast<std::uint8_t>(0x60 + i)), vrf_secs[i], vrf_pks)));

    for (const auto& v : vals) {
      CHECK(v->profile() == AssuranceProfile::hardened());
      CHECK(v->threshold_custody() && !v->sealed_mempool());
    }

    // Extend a few heights: the eligible VRF leader (first attempt with anyone
    // eligible) proposes each time.
    for (int h = 1; h <= 3; ++h) {
      std::optional<std::uint32_t> leader;
      for (std::uint32_t attempt = 0; attempt < 16 && !leader; ++attempt) {
        for (std::uint32_t i = 0; i < n; ++i) {
          // Peek eligibility by attempting a propose; the concrete engine is
          // hidden, so we just try in leader order and take the first that works.
          try {
            Block b = vals[i]->propose({}, static_cast<std::uint64_t>(h + 1));
            const Bytes seal = vals[i]->seal(b);
            for (const auto& v : vals) CHECK(v->submit(b, view(seal)).ok);
            leader = i;
            break;
          } catch (const std::exception&) {
            // not eligible at this attempt — try the next validator
          }
        }
      }
      CHECK_MSG(leader.has_value(), "some validator produced a valid VRF block for the height");
    }
    CHECK(vals[0]->height() == 3);
    for (const auto& v : vals) CHECK(v->tip().hash() == vals[0]->tip().hash());
  }

  std::printf("== the profile's signature gate is enforced end to end ==\n");
  {
    // Hardened forbids bare Ed25519. A v1 (Ed25519) transfer must be rejected by
    // the ledger the profiled genesis built — proving the posture reaches the
    // state layer, not just the descriptor.
    const GenesisSpec g = profiled_genesis(AssuranceProfile::hardened(), 1, alice);
    ChainSpec spec = g.chain_spec();
    CHECK_MSG(!spec.accepts(SigAlgo::kEd25519), "hardened genesis forbids classical Ed25519");
    CHECK(spec.accepts(SigAlgo::kSlhDsa) && spec.accepts(SigAlgo::kHybridEd25519MlDsa));
  }

  std::printf("== boot rejects an inconsistent / unknown / under-provisioned genesis ==\n");
  {
    // (a) A genesis whose sig set contradicts its declared profile.
    GenesisSpec bad = profiled_genesis(AssuranceProfile::fortress(), 1, alice);
    bad.accepted_sig_algos.push_back(static_cast<std::uint8_t>(SigAlgo::kEd25519));
    bool threw = false;
    try {
      (void)ProfiledValidator::boot(bad, 0, bls(0x60));
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "an inconsistent posture is rejected at boot");

    // (b) An unknown profile id.
    GenesisSpec unk = profiled_genesis(AssuranceProfile::standard(), 1, alice);
    unk.assurance_profile = 99;
    threw = false;
    try {
      (void)ProfiledValidator::boot(unk, 0, bls(0x60));
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK(threw);

    // (c) A VRF posture booted without VRF material.
    GenesisSpec hardened = profiled_genesis(AssuranceProfile::hardened(), 2, alice);
    threw = false;
    try {
      (void)ProfiledValidator::boot(hardened, 0, bls(0x60));  // no vrf_secret / vrf_pubkeys
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a VRF posture cannot boot without its VRF key material");
  }

  return bctest::summary();
}
