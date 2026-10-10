// AssuranceProfile: one named posture that fixes a consistent security tier at
// every seam, committed into the genesis hash so the whole network provably runs
// the same configuration — no node can silently join with a weaker one.
#include <vector>

#include "aviary/assurance.hpp"
#include "aviary/genesis.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using Level = AssuranceProfile::Level;
using Transport = AssuranceProfile::Transport;
using Consensus = AssuranceProfile::Consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

GenesisSpec base_spec() {
  GenesisSpec g;
  g.chain_id = 7;
  g.timestamp = 1;
  g.accounts.push_back(GenesisAccount{Hash{}, 1000});
  return g;
}
}  // namespace

int main() {
  init();

  std::printf("== each level maps to a consistent tier at every seam ==\n");
  {
    AssuranceProfile s = AssuranceProfile::standard();
    CHECK(s.transport() == Transport::kMutual);
    CHECK(s.consensus() == Consensus::kStakeWeighted);
    CHECK(!s.threshold_custody() && !s.sealed_mempool());

    AssuranceProfile h = AssuranceProfile::hardened();
    CHECK(h.transport() == Transport::kRatcheted);
    CHECK(h.consensus() == Consensus::kVrf);
    CHECK(h.threshold_custody() && !h.sealed_mempool());

    AssuranceProfile f = AssuranceProfile::fortress();
    CHECK(f.transport() == Transport::kRatcheted);
    CHECK(f.consensus() == Consensus::kVrf);
    CHECK(f.threshold_custody() && f.sealed_mempool());

    // Stronger levels never permit bare classical Ed25519.
    auto has_ed = [](const std::vector<std::uint8_t>& a) {
      for (auto x : a)
        if (x == static_cast<std::uint8_t>(SigAlgo::kEd25519)) return true;
      return false;
    };
    CHECK(has_ed(s.required_sig_algos()));
    CHECK(!has_ed(h.required_sig_algos()));
    CHECK(!has_ed(f.required_sig_algos()));
  }

  std::printf("== the posture is part of the chain identity (changes the genesis hash) ==\n");
  {
    GenesisSpec a = base_spec();
    GenesisSpec b = base_spec();
    AssuranceProfile::hardened().apply_to(a);
    AssuranceProfile::fortress().apply_to(b);
    CHECK_MSG(a.hash() != b.hash(), "two postures over the same accounts yield different chains");

    // apply_to also fixes the accepted signature set to the mandated one.
    CHECK(a.accepted_sig_algos == AssuranceProfile::hardened().required_sig_algos());
    CHECK(a.assurance_profile == AssuranceProfile::hardened().id());
  }

  std::printf("== round-trip: from_genesis recovers exactly the applied profile ==\n");
  {
    for (AssuranceProfile p :
         {AssuranceProfile::standard(), AssuranceProfile::hardened(), AssuranceProfile::fortress()}) {
      GenesisSpec g = base_spec();
      p.apply_to(g);
      CHECK(AssuranceProfile::from_genesis(g) == p);
      // Survives an encode/decode of the whole spec (profile is in the codec).
      GenesisSpec back = GenesisSpec::decode_all(view(g.encode()));
      CHECK(back.assurance_profile == g.assurance_profile);
      CHECK(AssuranceProfile::from_genesis(back) == p);
    }
  }

  std::printf("== inconsistency is rejected: a strong claim over weak parameters ==\n");
  {
    GenesisSpec g = base_spec();
    AssuranceProfile::fortress().apply_to(g);
    // Tamper: keep the fortress id but re-permit bare Ed25519.
    g.accepted_sig_algos.push_back(static_cast<std::uint8_t>(SigAlgo::kEd25519));
    bool threw = false;
    try {
      (void)AssuranceProfile::from_genesis(g);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK_MSG(threw, "a genesis whose sig set contradicts its declared profile is rejected");
  }

  std::printf("== an unknown profile id is rejected ==\n");
  {
    GenesisSpec g = base_spec();
    g.assurance_profile = 99;
    bool threw = false;
    try {
      (void)AssuranceProfile::from_genesis(g);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK(threw);
  }

  std::printf("== a chain built from a profiled genesis carries the mandated params ==\n");
  {
    GenesisSpec g = base_spec();
    AssuranceProfile::hardened().apply_to(g);
    ChainSpec spec = g.chain_spec();
    // The hardened profile forbids bare Ed25519 but accepts the PQ hybrid.
    CHECK(!spec.accepts(SigAlgo::kEd25519));
    CHECK(spec.accepts(SigAlgo::kHybridEd25519MlDsa));
    CHECK(spec.accepts(SigAlgo::kSlhDsa));
  }

  return bctest::summary();
}
