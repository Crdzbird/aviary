// Proof-of-work reference engine: a second consensus family behind the same
// ConsensusEngine seam. A block is justified by work — its header hash must meet
// a difficulty target, with the nonce carried in consensus_data.
#include <memory>
#include <vector>

#include "blockchainer/account_state.hpp"
#include "blockchainer/ledger.hpp"
#include "blockchainer/pow_engine.hpp"
#include "blockchainer/version.hpp"
#include "check.hpp"

using namespace blockchainer;
using namespace blockchainer::consensus;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
constexpr std::uint64_t kChain = 7;

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce, codec::u128 amt) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}
const AccountStateMachine& acc(const Ledger& l) {
  return dynamic_cast<const AccountStateMachine&>(l.state());
}

// An easy target: the header hash's first byte must be 0x00 (~256 tries).
Hash easy_target() {
  Hash t;
  t.fill(0xFF);
  t[0] = 0x00;
  return t;
}
}  // namespace

int main() {
  init();
  auto alice = ed(1);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(ed(2).public_key));

  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000);
  PowEngine engine(easy_target(), std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g)));

  std::printf("== mine a block and submit its proof of work ==\n");
  {
    Block b1 = engine.propose({transfer(alice, bob_addr, 0, 100)}, 1);
    CHECK(engine.mine(b1));  // finds a solving nonce
    CHECK(PowEngine::meets_target(b1.header.hash(), engine.target()));
    CHECK(b1.header.hash()[0] == 0x00);  // meets the easy target
    CHECK_MSG(engine.submit(b1, {}).ok, "mined block accepted");
    CHECK(engine.height() == 1);
    CHECK(acc(engine.ledger()).account(bob_addr).balance == 100);
  }

  std::printf("== a block with insufficient work is rejected ==\n");
  {
    Block b2 = engine.propose({transfer(alice, bob_addr, 1, 50)}, 2);
    // Force an unsolved nonce whose hash does NOT meet the target.
    PowData d{engine.target(), 0};
    for (std::uint64_t n = 0;; ++n) {
      d.nonce = n;
      b2.header.consensus_data = d.encode();
      if (!PowEngine::meets_target(b2.header.hash(), engine.target())) break;
    }
    BlockError e = engine.submit(b2, {});
    CHECK(!e.ok);
    CHECK(engine.height() == 1);  // unchanged
  }

  std::printf("== a block claiming the wrong target is rejected ==\n");
  {
    Block b2 = engine.propose({transfer(alice, bob_addr, 1, 50)}, 2);
    Hash wrong = engine.target();
    wrong[1] = 0x00;  // a different (harder) target than the engine's
    // Mine against the wrong target so the hash still looks "worked".
    PowData d{wrong, 0};
    for (std::uint64_t n = 0; n < (1u << 20); ++n) {
      d.nonce = n;
      b2.header.consensus_data = d.encode();
      if (PowEngine::meets_target(b2.header.hash(), wrong)) break;
    }
    CHECK(!engine.submit(b2, {}).ok);  // target mismatch
  }

  std::printf("== drive a chain through the abstract ConsensusEngine interface ==\n");
  {
    ConsensusEngine& ce = engine;
    Block b2 = ce.propose({transfer(alice, bob_addr, 1, 50)}, 2);
    // The pipeline mines via the concrete engine, then submits via the seam.
    CHECK(engine.mine(b2));
    CHECK_MSG(ce.submit(b2, {}).ok, "second mined block accepted via interface");
    CHECK(ce.height() == 2);
    CHECK(acc(engine.ledger()).account(alice_addr).balance == 850);
  }

  std::printf("== PowData encode/decode round-trip ==\n");
  {
    PowData d{easy_target(), 123456789};
    PowData back = PowData::decode_all(view(d.encode()));
    CHECK(back.target == d.target && back.nonce == d.nonce);
  }

  return bctest::summary();
}
