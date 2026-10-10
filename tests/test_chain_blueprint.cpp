// ChainBlueprint + ComponentRegistry + ChainAssembler: a chain is a MIXTURE of
// pluggable components named by stable ids, committed into the genesis hash.
// Built-ins assemble by id; a custom subclass of a seam registers under its
// own id and joins the mixture; anything unknown, or a mix the assurance
// posture forbids, is rejected at boot (fail-closed) — integrity preserved.
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "aviary/assurance.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/foreign_engine.hpp"
#include "aviary/genesis.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/pow_engine.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;
using node::ChainAssembler;
using node::ComponentRegistry;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }

// A single-validator genesis at [profile], funding one account.
GenesisSpec genesis(const AssuranceProfile& profile) {
  GenesisSpec g;
  g.chain_id = 7;
  g.timestamp = 1;
  Bytes seed(32, 1);
  g.accounts.push_back(GenesisAccount{crypto::address_of(view(crypto::ed25519_keygen_from_seed(view(seed)).public_key)), 1000});
  g.validators.push_back(consensus::Validator{bls(0x60).public_key, 1});
  profile.apply_to(g);
  return g;
}

// A CUSTOM engine — the extensibility proof. Subclasses the ConsensusEngine seam
// with the simplest possible justification (none) and rides the unchanged
// Ledger pipeline (build_block / process_block keep atomicity + determinism).
class TrivialEngine : public consensus::ConsensusEngine {
 public:
  explicit TrivialEngine(std::unique_ptr<Ledger> l) : ledger_(std::move(l)) {}
  const Ledger& ledger() const override { return *ledger_; }
  std::uint64_t height() const override { return ledger_->tip().height; }
  const BlockHeader& tip() const override { return ledger_->tip(); }
  Block propose(std::vector<Transaction> txs, std::uint64_t ts) override {
    return ledger_->build_block(std::move(txs), ts);
  }
  BlockError submit(const Block& b, codec::ByteView) override { return ledger_->process_block(b); }
 private:
  std::unique_ptr<Ledger> ledger_;
};

template <class F>
bool throws(F&& f) { return ::bctest::throws(std::forward<F>(f)); }
}  // namespace

int main() {
  init();

  // ── Blueprint codec + genesis commitment (the integrity anchor) ────────────
  {
    ChainBlueprint b;
    b.consensus_id = "pos";
    b.state_model_id = "account";
    b.proposer_id = "round-robin";
    b.component_params = Bytes{1, 2, 3};
    CHECK(ChainBlueprint::decode_all(view(b.encode())) == b);
    CHECK(b.hash() != crypto::Hash{});

    GenesisSpec plain = genesis(AssuranceProfile::standard());
    CHECK(!ChainBlueprint::from_genesis(plain).has_value());  // no mixture committed
    GenesisSpec mixed = plain;
    b.commit_to(mixed);
    CHECK(mixed.hash() != plain.hash());  // the MIXTURE is chain identity
    CHECK(GenesisSpec::decode_all(view(mixed.encode())).hash() == mixed.hash());
    CHECK(*ChainBlueprint::from_genesis(mixed) == b);
    // Orthogonal: assurance + membership fields untouched by committing a mixture.
    CHECK(mixed.assurance_profile == plain.assurance_profile && mixed.membership_policy.empty());
  }

  // ── Registry discovery: the built-ins are always nameable ──────────────────
  {
    auto has = [](const std::vector<std::string>& v, const char* s) {
      for (const auto& x : v) if (x == s) return true;
      return false;
    };
    auto e = ComponentRegistry::engine_ids();
    CHECK(has(e, "bft") && has(e, "pow") && has(e, "pos") && has(e, "vrf-pos"));
    auto s = ComponentRegistry::state_ids();
    CHECK(has(s, "account") && has(s, "utxo"));
    auto p = ComponentRegistry::proposer_ids();
    CHECK(has(p, "stake-weighted") && has(p, "round-robin"));
  }

  // ── Default assembly: a genesis WITHOUT a blueprint still assembles ────────
  {
    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint d = ChainAssembler::default_blueprint(g);
    CHECK(d.consensus_id == "pos" && d.state_model_id == "account");
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60));
    CHECK(chain.blueprint == d);
    CHECK(chain.engine->height() == 0);
  }

  // ── Built-in PoS via an explicit blueprint (+ a chosen proposer policy) ─────
  {
    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint b;
    b.consensus_id = "pos";
    b.state_model_id = "account";
    b.proposer_id = "round-robin";
    b.commit_to(g);
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60));
    auto* pos = dynamic_cast<consensus::PosEngine*>(chain.engine.get());
    CHECK(pos != nullptr);
    Block blk = chain.engine->propose({}, 2);
    CHECK(chain.engine->submit(blk, view(pos->seal(blk))).ok);
    CHECK(chain.engine->height() == 1);
  }

  // ── Built-in PoW via a blueprint carrying a difficulty target in params ─────
  {
    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint b;
    b.consensus_id = "pow";
    b.state_model_id = "account";
    b.component_params = Bytes(32, 0xFF);  // easiest target
    b.commit_to(g);
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60));
    auto* pow = dynamic_cast<consensus::PowEngine*>(chain.engine.get());
    CHECK(pow != nullptr);
    Block blk = chain.engine->propose({}, 2);
    CHECK(pow->mine(blk));
    CHECK(chain.engine->submit(blk, {}).ok);
    CHECK(chain.engine->height() == 1);
  }

  // ── EXTENSIBILITY: a custom engine subclass joins the mixture by id ────────
  {
    ComponentRegistry::register_engine(
        "trivial",
        [](node::BootContext& c) -> std::unique_ptr<consensus::ConsensusEngine> {
          return std::make_unique<TrivialEngine>(std::move(c.ledger));
        },
        /*grind_proof=*/false);
    auto ids = ComponentRegistry::engine_ids();
    bool listed = false;
    for (const auto& x : ids) listed |= (x == "trivial");
    CHECK(listed);

    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint b;
    b.consensus_id = "trivial";
    b.state_model_id = "account";
    b.commit_to(g);
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60));
    CHECK(dynamic_cast<TrivialEngine*>(chain.engine.get()) != nullptr);
    Block blk = chain.engine->propose({}, 2);
    CHECK(chain.engine->submit(blk, {}).ok);  // rides the unchanged Ledger pipeline
    CHECK(chain.engine->height() == 1);

    // A custom proposer policy plugs into a built-in engine the same way.
    ComponentRegistry::register_proposer("always-zero", [] {
      return consensus::ProposerPolicy([](std::uint64_t, const consensus::ValidatorSet&) { return 0u; });
    });
    GenesisSpec g2 = genesis(AssuranceProfile::standard());
    ChainBlueprint b2;
    b2.consensus_id = "pos";
    b2.state_model_id = "account";
    b2.proposer_id = "always-zero";
    b2.commit_to(g2);
    CHECK(ChainAssembler::assemble(g2, 0, bls(0x60)).engine->height() == 0);
  }

  // ── FOREIGN components: justification + proposer policies supplied from
  //    outside C++ (here: lambdas standing in for FFI callbacks) ─────────────
  {
    // Justification = the block's own header hash; verify = it must match.
    node::register_foreign_engine(
        "foreign-hash",
        [](const Block& b) { const auto h = b.header.hash(); return Bytes(h.begin(), h.end()); },
        [](const Block& b, crypto::ByteView j) {
          const auto h = b.header.hash();
          return j.size() == h.size() && std::equal(h.begin(), h.end(), j.begin());
        },
        /*grind_proof=*/false);
    node::register_foreign_proposer("foreign-last", [](std::uint64_t, std::uint32_t n) { return n - 1; });

    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint b;
    b.consensus_id = "foreign-hash";
    b.state_model_id = "account";
    b.commit_to(g);
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60));
    auto* fe = dynamic_cast<node::ForeignEngine*>(chain.engine.get());
    CHECK(fe != nullptr);
    Block blk = chain.engine->propose({}, 2);
    Bytes just = fe->justify(blk);
    CHECK(just.size() == 32);
    // A tampered justification is refused by the FOREIGN verifier — and the
    // ledger is untouched (fail-closed, atomic).
    Bytes bad = just; bad[0] ^= 1;
    CHECK(!chain.engine->submit(blk, view(bad)).ok);
    CHECK(chain.engine->height() == 0);
    CHECK(chain.engine->submit(blk, view(just)).ok);
    CHECK(chain.engine->height() == 1);

    // A foreign proposer plugs into a built-in engine.
    GenesisSpec g2 = genesis(AssuranceProfile::standard());
    ChainBlueprint b2;
    b2.consensus_id = "pos";
    b2.state_model_id = "account";
    b2.proposer_id = "foreign-last";
    b2.commit_to(g2);
    CHECK(ChainAssembler::assemble(g2, 0, bls(0x60)).engine->height() == 0);
  }

  // ── FAIL-CLOSED: unknown ids are rejected, never substituted ───────────────
  {
    GenesisSpec g = genesis(AssuranceProfile::standard());
    ChainBlueprint b;
    b.consensus_id = "no-such-engine";
    b.state_model_id = "account";
    b.commit_to(g);
    CHECK(throws([&] { (void)ChainAssembler::assemble(g, 0, bls(0x60)); }));

    b.consensus_id = "pos";
    b.state_model_id = "no-such-state";
    b.commit_to(g);
    CHECK(throws([&] { (void)ChainAssembler::assemble(g, 0, bls(0x60)); }));

    b.state_model_id = "account";
    b.proposer_id = "no-such-proposer";
    b.commit_to(g);
    CHECK(throws([&] { (void)ChainAssembler::assemble(g, 0, bls(0x60)); }));
  }

  // ── FAIL-CLOSED: the posture constrains the mix ────────────────────────────
  {
    // A hardened (VRF) posture forbids a predictable-leader engine.
    GenesisSpec g = genesis(AssuranceProfile::hardened());
    ChainBlueprint b;
    b.consensus_id = "pos";
    b.state_model_id = "account";
    b.commit_to(g);
    CHECK(throws([&] { (void)ChainAssembler::assemble(g, 0, bls(0x60)); }));

    // ...and permits the grind-proof one (given VRF material).
    Bytes vrf_seed(32, 0x42);
    Bytes vrf_pk = crypto::ecvrf_public_key(view(vrf_seed));
    b.consensus_id = "vrf-pos";
    b.commit_to(g);
    auto chain = ChainAssembler::assemble(g, 0, bls(0x60), vrf_seed, {vrf_pk});
    CHECK(chain.blueprint.consensus_id == "vrf-pos");
    CHECK(chain.engine->height() == 0);
  }

  // ── FAIL-CLOSED: the state id must match how the genesis is seeded ─────────
  {
    GenesisSpec g = genesis(AssuranceProfile::standard());  // seeded as ACCOUNT
    ChainBlueprint b;
    b.consensus_id = "pos";
    b.state_model_id = "utxo";
    b.commit_to(g);
    CHECK(throws([&] { (void)ChainAssembler::assemble(g, 0, bls(0x60)); }));
  }

  return bctest::summary();
}
