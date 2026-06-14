#include <vector>

#include "blockchainer/interop/bridge.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::interop::bridge;
using consensus::Validator;
using consensus::ValidatorSet;

namespace {
codec::ByteView view(const std::vector<std::uint8_t>& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { std::vector<std::uint8_t> ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
Hash tag(std::uint8_t b) { Hash h{}; h.fill(b); return h; }

// Collect a quorum attestation over a packet from the attestor set.
Attestation attest(const ValidatorSet& set, const std::vector<crypto::KeyPair>& keys,
                   const Packet& p, std::size_t n) {
  std::vector<AttestationVote> votes;
  for (std::size_t i = 0; i < n; ++i) votes.push_back(sign_attestation(p.commitment(), static_cast<std::uint32_t>(i), keys[i]));
  auto a = aggregate_attestation(set, p.commitment(), votes);
  return a.value();
}
}  // namespace

int main() {
  init();

  // 4 bridge attestors, power 1 each → quorum 3.
  std::vector<crypto::KeyPair> keys = {bls(10), bls(11), bls(12), bls(13)};
  std::vector<Validator> vs;
  for (auto& k : keys) vs.push_back(Validator{k.public_key, 1});
  ValidatorSet attestors(vs);

  const Hash asset = tag(0xA5);
  const Hash bob = tag(0xB0);
  const Hash alice = tag(0xA1);
  constexpr std::uint64_t kChainA = 1, kChainB = 2;

  BridgeEndpoint chainA(kChainA, attestors);
  BridgeEndpoint chainB(kChainB, attestors);

  std::printf("== lock on A → mint on B ==\n");
  Packet lockPkt = chainA.lock(kChainB, asset, 100, bob);
  CHECK(chainA.escrow(asset) == 100);
  CHECK(lockPkt.dest_chain == kChainB);

  Attestation att = attest(attestors, keys, lockPkt, 3);
  BridgeResult r = chainB.receive(lockPkt, att);
  CHECK_MSG(r.ok, r.ok ? "minted" : r.error.c_str());
  CHECK(chainB.minted_balance(asset, bob) == 100);
  CHECK(chainB.processed(kChainA, lockPkt.nonce));

  std::printf("== packet round-trips canonically ==\n");
  CHECK(Packet::decode_all(view(lockPkt.encode())).commitment() == lockPkt.commitment());

  std::printf("== adversarial ==\n");
  // Replay rejected.
  CHECK(!chainB.receive(lockPkt, att).ok);
  CHECK(chainB.minted_balance(asset, bob) == 100);  // unchanged

  // Below quorum → no attestation forms.
  {
    std::vector<AttestationVote> two = {
        sign_attestation(lockPkt.commitment(), 0, keys[0]),
        sign_attestation(lockPkt.commitment(), 1, keys[1])};
    CHECK(!aggregate_attestation(attestors, lockPkt.commitment(), two).has_value());
  }
  // Build throwaway packets directly (no escrow side effect from lock()).
  auto make_pkt = [&](std::uint64_t dest, std::uint64_t nonce) {
    Packet p;
    p.source_chain = kChainA;
    p.dest_chain = dest;
    p.nonce = nonce;
    p.payload = TransferPayload{TransferKind::kMint, asset, 5, bob}.encode();
    return p;
  };
  // Tampered attestation rejected.
  {
    Packet p2 = make_pkt(kChainB, 900);
    Attestation bad = attest(attestors, keys, p2, 3);
    bad.agg_signature[0] ^= 0x01;
    CHECK(!chainB.receive(p2, bad).ok);
  }
  // Wrong destination chain rejected.
  {
    Packet p3 = make_pkt(kChainB, 901);
    Attestation a3 = attest(attestors, keys, p3, 3);
    CHECK(!chainA.receive(p3, a3).ok);  // p3.dest is B, received on A
  }

  std::printf("== burn on B → release on A ==\n");
  {
    Packet burnPkt = chainB.burn(kChainA, asset, 40, alice);
    Attestation a = attest(attestors, keys, burnPkt, 4);  // all 4 attest
    BridgeResult rr = chainA.receive(burnPkt, a);
    CHECK_MSG(rr.ok, rr.ok ? "released" : rr.error.c_str());
    CHECK(chainA.escrow(asset) == 100 - 40);          // escrow reduced
    CHECK(chainA.released_balance(asset, alice) == 40);
  }

  std::printf("== release beyond escrow rejected ==\n");
  {
    Packet burnPkt = chainB.burn(kChainA, asset, 1000, alice);
    Attestation a = attest(attestors, keys, burnPkt, 3);
    CHECK(!chainA.receive(burnPkt, a).ok);
    CHECK(chainA.escrow(asset) == 60);  // unchanged
  }

  return bctest::summary();
}
