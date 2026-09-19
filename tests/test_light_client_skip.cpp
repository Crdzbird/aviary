// The light-client OVERLAP (skipping) rule + bisection: a client that missed
// many headers jumps ahead when the new header's certificate is signed by more
// than 1/3 of the set it already trusts; a full validator replacement forces a
// bisection through the rotation header. Every tamper is refused.
#include <cstdio>
#include <map>
#include <memory>

#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/light_client.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;
using blockchainer::client::LightClient;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls_key(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }
constexpr std::uint64_t kChain = 11;

// A BFT chain whose validator set we can rotate; records every finalized
// checkpoint (header, cert, set in force at that height) for the light client.
struct Chain {
  std::vector<crypto::KeyPair> keys;          // keys[i] signs as index i of the CURRENT set
  std::unique_ptr<BftEngine> engine;
  std::map<std::uint64_t, LightClient::Checkpoint> checkpoints;
  BlockHeader genesis;
  ValidatorSet genesis_set;

  static ValidatorSet set_of(const std::vector<crypto::KeyPair>& k) {
    std::vector<Validator> vs;
    for (auto& kp : k) vs.push_back(Validator{kp.public_key, 1});
    return ValidatorSet(vs);
  }
  explicit Chain(std::vector<crypto::KeyPair> k) : keys(std::move(k)), genesis_set(set_of(keys)) {
    auto st = std::make_unique<AccountStateMachine>();
    st->credit_genesis(Hash{}, 1000);
    engine = std::make_unique<BftEngine>(genesis_set, std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(st)));
    genesis = engine->tip();
  }
  // Finalize the next (empty) block, signed by the whole current set.
  const LightClient::Checkpoint& next(std::uint64_t ts) {
    Block b = engine->propose({}, ts);
    const ValidatorSet in_force = engine->validators();
    std::vector<Vote> votes;
    for (std::uint32_t i = 0; i < in_force.size(); ++i) votes.push_back(engine->precommit(b, i, keys[i]));
    auto cert = engine->try_commit(b, votes);
    if (!cert || !engine->finalize(b, *cert).ok) throw std::runtime_error("finalize failed");
    return checkpoints.insert_or_assign(b.header.height, LightClient::Checkpoint{b.header, *cert, in_force}).first->second;
  }
  // Rotate: the next block announces [new_keys] (each power 1); afterwards they sign.
  void rotate(std::vector<crypto::KeyPair> new_keys, std::uint64_t ts) {
    std::vector<Validator> vs;
    for (auto& kp : new_keys) vs.push_back(Validator{kp.public_key, 1});
    engine->schedule_validator_set(ValidatorSet(vs));
    next(ts);                 // announced + certified by the OLD set
    keys = std::move(new_keys);
  }
  std::function<std::optional<LightClient::Checkpoint>(std::uint64_t)> fetcher() const {
    return [this](std::uint64_t h) -> std::optional<LightClient::Checkpoint> {
      auto it = checkpoints.find(h);
      if (it == checkpoints.end()) return std::nullopt;
      return it->second;
    };
  }
};
}  // namespace

int main() {
  init();
  std::vector<crypto::KeyPair> A = {bls_key(1), bls_key(2), bls_key(3), bls_key(4)};       // genesis set
  std::vector<crypto::KeyPair> B = {bls_key(5), bls_key(6), bls_key(7), bls_key(8)};       // a complete replacement
  std::vector<crypto::KeyPair> C = {bls_key(2), bls_key(3), bls_key(9), bls_key(10)};      // half overlap with A
  Chain chain(A);
  chain.next(2); chain.next(3); chain.next(4);            // heights 1-3 under A
  chain.rotate(C, 5);                                     // height 4 announces C (certified by A)
  chain.next(6); chain.next(7);                           // 5-6 under C
  chain.rotate(B, 8);                                     // height 7 announces B (certified by C)
  chain.next(9); chain.next(10); chain.next(11);          // 8-10 under B
  CHECK(chain.checkpoints.size() == 10);
  auto cp = [&](std::uint64_t h) -> const LightClient::Checkpoint& { return chain.checkpoints.at(h); };

  std::printf("== a skip of many heights under one set: 4/4 of the trusted set signed ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    LightClient::Result r = lc.accept_skipping(cp(3).header, cp(3).cert, cp(3).validators);
    CHECK_MSG(r.ok, r.error.c_str());
    CHECK(lc.height() == 3 && lc.validators().hash() == chain.genesis_set.hash());
    // A skip of exactly one behaves like accept (no linkage needed either).
    CHECK(lc.accept_skipping(cp(4).header, cp(4).cert, cp(4).validators).ok);
    CHECK(lc.validators().hash() == cp(5).validators.hash());   // announced C is in force next
    CHECK(lc.tip_validators().hash() == chain.genesis_set.hash());
  }

  std::printf("== half overlap (C shares 2 of A's 4) passes the 1/3 rule; 1/4 does not ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    // Jump from genesis (trusting A) straight to height 6 (certified by C): overlap = 2/4 > 1/3.
    LightClient::Result r = lc.accept_skipping(cp(6).header, cp(6).cert, cp(6).validators);
    CHECK_MSG(r.ok, r.error.c_str());
    CHECK(lc.height() == 6 && lc.tip_validators().hash() == cp(6).validators.hash());
    // With a stricter trust level (1/2) the same jump is too thin — and reports it as such.
    LightClient strict(kChain, chain.genesis_set, chain.genesis);
    strict.set_trust_level(1, 2);
    LightClient::Result s = strict.accept_skipping(cp(6).header, cp(6).cert, cp(6).validators);
    CHECK(!s.ok && s.insufficient_overlap);
    // Exactly 1/3 is NOT enough (strictly greater): trust 2/4 would need > 2 of 4.
    LightClient edge(kChain, chain.genesis_set, chain.genesis);
    edge.set_trust_level(2, 4);
    CHECK(!edge.accept_skipping(cp(6).header, cp(6).cert, cp(6).validators).ok);
    // Bad trust levels are ignored (level unchanged).
    edge.set_trust_level(0, 0); edge.set_trust_level(3, 2);
    CHECK(!edge.accept_skipping(cp(6).header, cp(6).cert, cp(6).validators).ok);
  }

  std::printf("== a complete replacement: no overlap → thin, not a hard failure ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    LightClient::Result r = lc.accept_skipping(cp(10).header, cp(10).cert, cp(10).validators);
    CHECK(!r.ok && r.insufficient_overlap);
    CHECK(lc.height() == 0);                                   // untouched
  }

  std::printf("== bisection syncs from genesis to height 10 through both rotations ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    std::vector<std::uint64_t> fetched;
    auto fetch = [&](std::uint64_t h) { fetched.push_back(h); return chain.fetcher()(h); };
    LightClient::Result r = lc.sync_to(cp(10), fetch);
    CHECK_MSG(r.ok, r.error.c_str());
    CHECK(lc.height() == 10 && lc.tip().hash() == cp(10).header.hash());
    CHECK(lc.tip_validators().hash() == cp(10).validators.hash());
    CHECK(!fetched.empty());                                   // it had to bisect
    for (std::uint64_t h : fetched) CHECK(h > 0 && h < 10);
    // The synced client continues sequentially like any other.
    CHECK(lc.accept(chain.next(12).header, cp(11).cert).ok && lc.height() == 11);
  }

  std::printf("== bisection fails closed when a needed checkpoint is unavailable ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    auto none = [](std::uint64_t) -> std::optional<LightClient::Checkpoint> { return std::nullopt; };
    LightClient::Result r = lc.sync_to(cp(10), none);
    CHECK(!r.ok && !r.insufficient_overlap && lc.height() == 0);
    // A fetcher lying about the height is refused.
    auto liar = [&](std::uint64_t) -> std::optional<LightClient::Checkpoint> { return cp(2); };
    CHECK(!lc.sync_to(cp(10), liar).ok);
    // Target not ahead.
    LightClient at10(kChain, cp(10).validators, cp(10).header, cp(10).validators);
    CHECK(!at10.sync_to(cp(10), chain.fetcher()).ok);
  }

  std::printf("== tampering is refused as a HARD failure (never 'thin overlap') ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    auto hard = [&](const char* what, LightClient::Result r) { CHECK_MSG(!r.ok && !r.insufficient_overlap, what); };
    hard("wrong set supplied (hash mismatch)", lc.accept_skipping(cp(6).header, cp(6).cert, chain.genesis_set));
    CommitCertificate bad = cp(6).cert; bad.agg_signature.back() ^= 1;
    hard("tampered aggregate", lc.accept_skipping(cp(6).header, bad, cp(6).validators));
    BlockHeader th = cp(6).header; th.state_root[0] ^= 1;
    hard("tampered header", lc.accept_skipping(th, cp(6).cert, cp(6).validators));
    hard("wrong chain", [&] { BlockHeader h = cp(6).header; h.chain_id = 99; return lc.accept_skipping(h, cp(6).cert, cp(6).validators); }());
    hard("not ahead", [&] { LightClient x(kChain, cp(6).validators, cp(6).header, cp(6).validators); return x.accept_skipping(cp(3).header, cp(3).cert, cp(3).validators); }());
    CommitCertificate stale = cp(5).cert;                       // cert for another height
    hard("certificate for another header", lc.accept_skipping(cp(6).header, stale, cp(6).validators));
    // A forged "set" that verifies the cert but is not what the header commits.
    std::vector<Validator> forged_vs;
    for (auto& k : C) forged_vs.push_back(Validator{k.public_key, 7});
    hard("set with altered powers", lc.accept_skipping(cp(6).header, cp(6).cert, ValidatorSet(forged_vs)));
    // max_skip bounds a single jump.
    lc.set_max_skip(2);
    hard("skip exceeds max_skip", lc.accept_skipping(cp(3).header, cp(3).cert, cp(3).validators));
    CHECK(lc.accept_skipping(cp(2).header, cp(2).cert, cp(2).validators).ok);
    CHECK(lc.height() == 0 + 2);
  }

  std::printf("== a skipped-to header is as good as a walked-to one for proofs ==\n");
  {
    LightClient lc(kChain, chain.genesis_set, chain.genesis);
    CHECK(lc.sync_to(cp(10), chain.fetcher()).ok);
    const auto& acc = dynamic_cast<const AccountStateMachine&>(chain.engine->ledger().state());
    // State moved on (height 11) — prove against the height-10 root via the chain's own history is
    // not available here, so prove at the CURRENT tip after walking one more header.
    CHECK(lc.accept(cp(11).header, cp(11).cert).ok);
    StateProof p = acc.prove_account(Hash{});
    CHECK(verify_state_proof(lc.tip().state_root, p));
  }

  return bctest::summary();
}
