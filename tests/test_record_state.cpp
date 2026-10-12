// Record model: a signed key value store for application objects. Covers the
// state machine alone (ownership, versions, nonces, proofs, snapshots) and the
// full path through the assembler with a BFT engine.
#include <memory>
#include <string>
#include <vector>

#include "aviary/assurance.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/genesis.hpp"
#include "aviary/proofs.hpp"
#include "aviary/record_state.hpp"
#include "aviary/version.hpp"
#include "check.hpp"

using namespace aviary;

namespace {
codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }
Hash key_of(const std::string& id) { return crypto::blake3_256(reinterpret_cast<const std::uint8_t*>(id.data()), id.size()); }
Bytes bytes_of(const std::string& s) { return Bytes(s.begin(), s.end()); }

Transaction write(const crypto::KeyPair& kp, std::uint64_t nonce, const Hash& key,
                  RecordStateMachine::Op op, const Bytes& value, std::uint64_t chain_id = 7) {
  Transaction tx;
  tx.version = 1;
  tx.chain_id = chain_id;
  tx.body = RecordStateMachine::encode_write({nonce, key, static_cast<std::uint8_t>(op), value});
  sign_transaction(tx, kp);
  return tx;
}
}  // namespace

int main() {
  init();
  const crypto::KeyPair alice = crypto::ed25519_keygen_from_seed(view(Bytes(32, 0xA1)));
  const crypto::KeyPair bob = crypto::ed25519_keygen_from_seed(view(Bytes(32, 0xB0)));
  const Hash alice_addr = crypto::address_of(view(alice.public_key));
  const Hash bob_addr = crypto::address_of(view(bob.public_key));
  const Hash part = key_of("part:ABC-123");

  // -- The state machine alone -------------------------------------------------
  {
    RecordStateMachine sm;
    const Hash empty_root = sm.state_root();
    Transaction create = write(alice, 0, part, RecordStateMachine::kPut, bytes_of("{\"serial\":\"ABC-123\",\"status\":\"built\"}"));
    CHECK(sm.validate(create).ok);
    sm.apply(create);
    CHECK(sm.record(part) != nullptr && sm.record(part)->version == 1 && sm.record(part)->owner == alice_addr);
    CHECK(sm.nonce(alice_addr) == 1);
    CHECK(sm.state_root() != empty_root);

    // Replay and wrong author are refused.
    CHECK(!sm.validate(create).ok);
    Transaction hijack = write(bob, 0, part, RecordStateMachine::kPut, bytes_of("mine now"));
    CHECK(!sm.validate(hijack).ok);

    // The owner updates: the version grows, the owner stays.
    Transaction update = write(alice, 1, part, RecordStateMachine::kPut, bytes_of("{\"status\":\"shipped\"}"));
    CHECK(sm.validate(update).ok);
    sm.apply(update);
    CHECK(sm.record(part)->version == 2 && sm.record(part)->value == bytes_of("{\"status\":\"shipped\"}"));

    // Proof of the committed record, readable by the verifier.
    const StateProof proof = sm.prove_record(part);
    CHECK(proof.present && verify_state_proof(sm.state_root(), proof));
    const auto leaf = RecordStateMachine::decode_record_leaf(proof.leaf_preimage);
    CHECK(leaf.key == part && leaf.owner == alice_addr && leaf.version == 2);
    CHECK(!sm.prove_record(key_of("missing")).present);
    StateProof bad = proof;
    bad.leaf_preimage.back() ^= 1;
    CHECK(!verify_state_proof(sm.state_root(), bad));

    // Transfer: bob may then write, alice may not; malformed transfers refused.
    Transaction short_owner = write(alice, 2, part, RecordStateMachine::kTransfer, bytes_of("bob"));
    CHECK(!sm.validate(short_owner).ok);
    Transaction transfer = write(alice, 2, part, RecordStateMachine::kTransfer, Bytes(bob_addr.begin(), bob_addr.end()));
    CHECK(sm.validate(transfer).ok);
    sm.apply(transfer);
    CHECK(sm.record(part)->owner == bob_addr && sm.record(part)->version == 3);
    CHECK(!sm.validate(write(alice, 3, part, RecordStateMachine::kPut, bytes_of("x"))).ok);
    Transaction bob_update = write(bob, 0, part, RecordStateMachine::kPut, bytes_of("{\"status\":\"installed\"}"));
    CHECK(sm.validate(bob_update).ok);
    sm.apply(bob_update);

    // Snapshot round trip is byte identical.
    const Bytes blob = sm.serialize();
    auto back = RecordStateMachine::deserialize(view(blob));
    CHECK(back->state_root() == sm.state_root() && back->serialize() == blob);
    CHECK(deserialize_state(StateModel::kRecord, view(blob))->state_root() == sm.state_root());

    // Delete, then the key is free for anyone again.
    CHECK(!sm.validate(write(bob, 1, part, RecordStateMachine::kDelete, bytes_of("x"))).ok);
    Transaction del = write(bob, 1, part, RecordStateMachine::kDelete, {});
    CHECK(sm.validate(del).ok);
    sm.apply(del);
    CHECK(sm.record(part) == nullptr && sm.size() == 0);
    CHECK(sm.validate(write(alice, 3, part, RecordStateMachine::kPut, bytes_of("again"))).ok);

    // Oversized values are refused; unknown ops are refused.
    CHECK(!sm.validate(write(alice, 3, part, RecordStateMachine::kPut, Bytes(RecordStateMachine::kMaxValue + 1, 1))).ok);
    CHECK(!sm.validate(write(alice, 3, part, static_cast<RecordStateMachine::Op>(9), {})).ok);
  }

  // -- Through the assembler: a BFT record chain -------------------------------
  {
    GenesisSpec g;
    g.chain_id = 7;
    g.timestamp = 1;
    g.state_model = StateModel::kRecord;
    AssuranceProfile::standard().apply_to(g);
    Bytes ikm(32, 0x60);
    const crypto::KeyPair bls = crypto::bls_keygen_from_ikm(view(ikm));
    g.validators.push_back(consensus::Validator{bls.public_key, 1});
    ChainBlueprint b;
    b.consensus_id = "bft";
    b.state_model_id = "record";
    g.chain_blueprint = b.encode();
    auto chain = node::ChainAssembler::assemble(g, 0, bls);
    CHECK(node::ChainAssembler::default_blueprint(g).state_model_id == "record");

    std::vector<Transaction> txs;
    txs.push_back(write(alice, 0, part, RecordStateMachine::kPut, bytes_of("v1")));
    txs.push_back(write(alice, 1, part, RecordStateMachine::kPut, bytes_of("v2")));
    Block blk = chain.engine->propose(txs, 2);
    auto* bft = dynamic_cast<consensus::BftEngine*>(chain.engine.get());
    CHECK(bft != nullptr);
    const auto idx = bft->validators().index_of(bls.public_key);
    CHECK(idx.has_value());
    const consensus::Vote vote = bft->precommit(blk, *idx, bls);
    const auto cert = bft->try_commit(blk, {vote});
    CHECK(cert.has_value());
    CHECK(chain.engine->submit(blk, view(cert->encode())).ok && chain.engine->height() == 1);
    const auto* rs = dynamic_cast<const RecordStateMachine*>(&chain.engine->ledger().state());
    CHECK(rs != nullptr && rs->record(part) != nullptr && rs->record(part)->version == 2);
    CHECK(blk.header.state_root == rs->state_root());
    CHECK(verify_state_proof(blk.header.state_root, rs->prove_record(part)));

    // A mixture naming "account" over a record genesis is refused.
    ChainBlueprint wrong;
    wrong.consensus_id = "bft";
    wrong.state_model_id = "account";
    GenesisSpec g2 = g;
    g2.chain_blueprint = wrong.encode();
    CHECK(bctest::throws([&] { node::ChainAssembler::assemble(g2, 0, bls); }));
  }
  return bctest::summary();
}
