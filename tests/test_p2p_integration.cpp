// Two-node networking core over an in-memory, end-to-end-encrypted transport:
// tx gossip, finalized-block propagation, and range sync — exercising mempool +
// BFT engine + wire protocol + secure session together.
#include <map>
#include <memory>
#include <vector>

#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/mempool.hpp"
#include "aviary/secure_session.hpp"
#include "aviary/version.hpp"
#include "aviary/wire.hpp"
#include "check.hpp"

using namespace aviary;
using namespace aviary::consensus;
using p2p::SecureSession;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
crypto::KeyPair ed(std::uint8_t s) { Bytes seed(32, s); return crypto::ed25519_keygen_from_seed(view(seed)); }
crypto::KeyPair bls(std::uint8_t s) { Bytes ikm(32, s); return crypto::bls_keygen_from_ikm(view(ikm)); }

constexpr std::uint64_t kChain = 7;

struct Node {
  std::unique_ptr<BftEngine> engine;
  Mempool mempool;
  std::map<std::uint64_t, wire::BlockMsg> archive;  // finalized blocks for serving sync
  std::uint64_t height() const { return engine->height(); }
  const StateMachine& state() const { return engine->ledger().state(); }
};

Node make_node(const ValidatorSet& vset, const Hash& alice_addr) {
  auto g = std::make_unique<AccountStateMachine>();
  g->credit_genesis(alice_addr, 1000);
  auto ledger = std::make_unique<Ledger>(ChainSpec{kChain, StateModel::kAccount}, std::move(g));
  return Node{std::make_unique<BftEngine>(vset, std::move(ledger)), Mempool{}, {}};
}

const AccountStateMachine& acc(const Node& n) {
  return dynamic_cast<const AccountStateMachine&>(n.state());
}

Transaction transfer(const crypto::KeyPair& from, const Hash& to, std::uint64_t nonce, codec::u128 amt) {
  Transaction tx; tx.version = 1; tx.chain_id = kChain;
  tx.body = AccountStateMachine::encode_transfer({nonce, to, amt});
  sign_transaction(tx, from);
  return tx;
}

// Proposer: build from mempool, self-finalize (single validator, quorum 1), archive + gossip.
wire::BlockMsg produce(Node& a, const crypto::KeyPair& vkey, std::uint64_t ts) {
  auto txs = a.mempool.select(a.state(), 100);
  Block blk = a.engine->propose(txs, ts);
  auto cert = a.engine->try_commit(blk, {a.engine->precommit(blk, 0, vkey)});
  a.engine->finalize(blk, *cert);
  std::vector<Hash> ids;
  for (const auto& t : blk.transactions) ids.push_back(t.txid());
  a.mempool.remove(ids);
  wire::BlockMsg bm{blk, *cert};
  a.archive[blk.header.height] = bm;
  return bm;
}

// Encrypt a message on the sender's session, decrypt + decode on the receiver's.
wire::Message transmit(SecureSession& from, SecureSession& to, const wire::Message& m) {
  Bytes frame = from.encrypt(m.encode());
  Bytes plain = to.decrypt(frame);
  return wire::Message::decode_all(view(plain));
}
}  // namespace

int main() {
  init();

  auto vkey = bls(100);  // the single validator's BLS key
  ValidatorSet vset(std::vector<Validator>{{vkey.public_key, 1}});  // quorum 1
  auto alice = ed(1), bob = ed(2);
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));

  Node A = make_node(vset, alice_addr);  // proposer/validator
  Node B = make_node(vset, alice_addr);  // follower

  std::printf("== mutually authenticated encrypted session A <-> B ==\n");
  auto idA = SecureSession::generate_identity();
  auto idB = SecureSession::generate_identity();
  auto [sa, msg1] = SecureSession::begin_initiator(idB.kem.public_key, idA.sig);
  auto [sb, msg2] = SecureSession::accept_responder(idB, msg1);
  sa.complete_initiator(msg2);
  CHECK(sa.open() && sb.open());
  CHECK(sb.peer_identity() == idA.sig.public_key);  // B authenticated A
  CHECK(sa.peer_identity() == idB.sig.public_key);  // A authenticated B

  std::printf("== tx gossip A -> B ==\n");
  Transaction t0 = transfer(alice, bob_addr, 0, 100);
  CHECK(A.mempool.add(t0, kChain) == AddResult::kAdded);
  {
    wire::Message got = transmit(sa, sb, wire::make(wire::TxMsg{t0}));
    B.mempool.add(wire::as_tx(got).tx, kChain);
  }
  CHECK(B.mempool.contains(t0.txid()));

  std::printf("== A produces + finalizes block 1 ==\n");
  wire::BlockMsg bm1 = produce(A, vkey, 1);
  CHECK(A.height() == 1);
  CHECK(acc(A).account(alice_addr).balance == 900);
  CHECK(acc(A).account(bob_addr).balance == 100);

  std::printf("== block gossip A -> B, B applies + verifies cert ==\n");
  {
    wire::Message got = transmit(sa, sb, wire::make(bm1));
    wire::BlockMsg rb = wire::as_block(got);
    BlockError be = B.engine->finalize(rb.block, rb.commit);
    CHECK_MSG(be.ok, be.ok ? "B finalized block 1" : be.error.c_str());
  }
  CHECK(B.height() == 1);
  CHECK(B.state().state_root() == A.state().state_root());  // identical state
  CHECK(acc(B).account(bob_addr).balance == 100);

  std::printf("== A produces block 2 ==\n");
  A.mempool.add(transfer(alice, bob_addr, 1, 50), kChain);
  produce(A, vkey, 2);
  CHECK(A.height() == 2);

  std::printf("== fresh node C syncs from genesis via GetBlocks ==\n");
  Node C = make_node(vset, alice_addr);
  CHECK(C.height() == 0);
  auto idAsync = SecureSession::generate_identity();  // A's identity for this link
  auto idC = SecureSession::generate_identity();      // C's identity
  auto [sc, c1] = SecureSession::begin_initiator(idAsync.kem.public_key, idC.sig);  // C → A
  auto [saC, c2] = SecureSession::accept_responder(idAsync, c1);
  sc.complete_initiator(c2);

  // C requests blocks 1.. ; A serves from its archive; C applies in order.
  wire::Message req = transmit(sc, saC, wire::make(wire::GetBlocks{1, 10}));
  wire::GetBlocks g = wire::as_get_blocks(req);
  for (std::uint64_t h = g.from_height; h < g.from_height + g.count; ++h) {
    auto it = A.archive.find(h);
    if (it == A.archive.end()) break;
    wire::Message got = transmit(saC, sc, wire::make(it->second));
    wire::BlockMsg rb = wire::as_block(got);
    BlockError be = C.engine->finalize(rb.block, rb.commit);
    CHECK_MSG(be.ok, be.ok ? "C applied a synced block" : be.error.c_str());
  }
  CHECK(C.height() == 2);
  CHECK(C.state().state_root() == A.state().state_root());
  CHECK(acc(C).account(alice_addr).balance == 850);
  CHECK(acc(C).account(bob_addr).balance == 150);

  return bctest::summary();
}
