// Deterministic fuzz-lite + round-trip property tests for the deserializers
// (the untrusted-input attack surface). Goal: on ANY bytes, every decoder
// either succeeds or throws CodecError — never any other exception, never a
// crash. Build with -DBLOCKCHAINER_SANITIZE=ON to run these under ASan/UBSan.
#include <cstdint>
#include <vector>

#include <string>

#include "blockchainer/account_state.hpp"
#include "blockchainer/block_store.hpp"
#include "blockchainer/consensus.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/interop/bridge.hpp"
#include "blockchainer/interop/encoding.hpp"
#include "blockchainer/interop/json.hpp"
#include "blockchainer/interop/rlp.hpp"
#include "blockchainer/pow_engine.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/utxo_state.hpp"
#include "blockchainer/version.hpp"
#include "blockchainer/wire.hpp"
#include "check.hpp"

using namespace blockchainer;

namespace {
// Seeded xorshift64 — deterministic, no std::rand / clock (determinism mandate).
struct Rng {
  std::uint64_t s;
  explicit Rng(std::uint64_t seed) : s(seed ? seed : 0x9e3779b97f4a7c15ull) {}
  std::uint64_t next() {
    s ^= s << 13;
    s ^= s >> 7;
    s ^= s << 17;
    return s;
  }
  std::uint32_t below(std::uint32_t n) { return n == 0 ? 0 : next() % n; }
};

Bytes random_bytes(Rng& rng, std::size_t len) {
  Bytes b(len);
  for (auto& x : b) x = static_cast<std::uint8_t>(rng.next());
  return b;
}

codec::ByteView view(const Bytes& b) { return codec::ByteView(b.data(), b.size()); }

// Returns true if [f] either returns normally or throws CodecError; false if it
// throws anything else (a contract violation).
template <class F>
bool total(F&& f) {
  try {
    f();
    return true;
  } catch (const codec::CodecError&) {
    return true;
  } catch (...) {
    return false;
  }
}
}  // namespace

int main() {
  init();

  std::printf("== decoder totality on arbitrary bytes (20k iterations) ==\n");
  Rng rng(0xC0DECAFEull);
  bool totality_ok = true;
  for (int i = 0; i < 20000; ++i) {
    Bytes b = random_bytes(rng, rng.below(257));
    auto v = view(b);
    totality_ok &= total([&] { (void)Transaction::decode_all(v); });
    totality_ok &= total([&] { (void)BlockHeader::decode_all(v); });
    totality_ok &= total([&] { (void)Block::decode_all(v); });
    totality_ok &= total([&] { (void)AccountStateMachine::decode_transfer(b); });
    totality_ok &= total([&] { (void)UtxoStateMachine::decode_body(b); });
    totality_ok &= total([&] { (void)UtxoStateMachine::decode_witnesses(b); });
    totality_ok &= total([&] { (void)Receipt::decode_all(v); });
    totality_ok &= total([&] { (void)AccountStateMachine::deserialize(v); });
    totality_ok &= total([&] { (void)UtxoStateMachine::deserialize(v); });
    totality_ok &= total([&] { (void)BlockStore::load(v); });
    totality_ok &= total([&] { (void)consensus::CommitCertificate::decode_all(v); });
    totality_ok &= total([&] { (void)consensus::ConsensusData::decode_all(v); });
    totality_ok &= total([&] { (void)consensus::Evidence::decode_all(v); });
    totality_ok &= total([&] { (void)consensus::PowData::decode_all(v); });
    totality_ok &= total([&] { (void)GenesisSpec::decode_all(v); });
    totality_ok &= total([&] { (void)wire::Message::decode_all(v); });
    // Typed wire payload decoders on arbitrary payloads.
    totality_ok &= total([&] { (void)wire::as_tx(wire::Message{wire::MessageType::kTx, b}); });
    totality_ok &= total([&] { (void)wire::as_block(wire::Message{wire::MessageType::kBlock, b}); });
    totality_ok &= total([&] { (void)wire::as_hello(wire::Message{wire::MessageType::kHello, b}); });
    totality_ok &= total([&] { (void)wire::as_get_blocks(wire::Message{wire::MessageType::kGetBlocks, b}); });
    totality_ok &= total([&] { (void)wire::as_evidence(wire::Message{wire::MessageType::kEvidence, b}); });
    totality_ok &= total([&] { (void)wire::as_consensus(wire::Message{wire::MessageType::kConsensus, b}); });
  }
  CHECK_MSG(totality_ok,
            "all decoders are total (success or CodecError only) on random input");

  std::printf("== interop decoders survive arbitrary input (20k iterations) ==\n");
  {
    // These throw various exception types; the property under test (and under
    // ASan/UBSan) is: no crash, no memory error, no unbounded recursion.
    auto run = [](auto&& f) { try { f(); } catch (...) {} };
    namespace ib = blockchainer::interop;
    for (int i = 0; i < 20000; ++i) {
      Bytes b = random_bytes(rng, rng.below(257));
      auto v = view(b);
      std::string s(b.begin(), b.end());
      run([&] { (void)ib::rlp::decode(v); });
      run([&] { (void)blockchainer::json::Value::parse(s); });
      run([&] { (void)ib::encoding::base58_decode(s); });
      run([&] { (void)ib::encoding::base64_decode(s); });
      run([&] { std::size_t p = 0; (void)ib::encoding::compact_u16_decode(b, p); });
      run([&] { (void)ib::bridge::Packet::decode_all(v); });
      run([&] { (void)ib::bridge::TransferPayload::decode_all(v); });
    }
    CHECK_MSG(true, "interop decoders did not crash on 140k random inputs");
  }

  std::printf("== transaction round-trip property (5k random values) ==\n");
  bool tx_roundtrip_ok = true;
  for (int i = 0; i < 5000; ++i) {
    Transaction tx;
    tx.version = static_cast<std::uint8_t>(rng.next());
    tx.chain_id = rng.next() >> 1;  // keep < 2^63
    tx.sig_algo = static_cast<std::uint8_t>(rng.next());
    tx.fee = (static_cast<codec::u128>(rng.next()) << 64) | rng.next();  // full-width fee
    tx.body = random_bytes(rng, rng.below(40));
    tx.author = random_bytes(rng, rng.below(40));
    tx.signature = random_bytes(rng, rng.below(72));
    Bytes enc = tx.encode();
    Transaction back = Transaction::decode_all(view(enc));
    if (back.encode() != enc) tx_roundtrip_ok = false;
  }
  CHECK_MSG(tx_roundtrip_ok, "decode(encode(tx)) re-encodes identically");

  std::printf("== header round-trip property (5k random values) ==\n");
  bool hdr_roundtrip_ok = true;
  for (int i = 0; i < 5000; ++i) {
    BlockHeader h;
    h.version = static_cast<std::uint8_t>(rng.next());
    h.chain_id = rng.next() >> 1;
    h.height = rng.next() >> 1;
    h.timestamp = rng.next() >> 1;
    for (auto& x : h.parent_hash) x = static_cast<std::uint8_t>(rng.next());
    for (auto& x : h.tx_root) x = static_cast<std::uint8_t>(rng.next());
    for (auto& x : h.state_root) x = static_cast<std::uint8_t>(rng.next());
    for (auto& x : h.receipt_root) x = static_cast<std::uint8_t>(rng.next());  // only serialized when version>=2
    h.consensus_data = random_bytes(rng, rng.below(48));
    Bytes enc = h.encode();
    BlockHeader back = BlockHeader::decode_all(view(enc));
    if (back.encode() != enc) hdr_roundtrip_ok = false;
  }
  CHECK_MSG(hdr_roundtrip_ok, "decode(encode(header)) re-encodes identically");

  std::printf("== equivocation-evidence round-trip property (5k random values) ==\n");
  bool ev_roundtrip_ok = true;
  for (int i = 0; i < 5000; ++i) {
    consensus::Evidence ev;
    ev.validator_index = static_cast<std::uint32_t>(rng.next());
    ev.height = rng.next() >> 1;
    ev.round = static_cast<std::uint32_t>(rng.next());
    for (auto& x : ev.block_id_a) x = static_cast<std::uint8_t>(rng.next());
    for (auto& x : ev.block_id_b) x = static_cast<std::uint8_t>(rng.next());
    ev.signature_a = random_bytes(rng, rng.below(100));
    ev.signature_b = random_bytes(rng, rng.below(100));
    Bytes enc = ev.encode();
    if (consensus::Evidence::decode_all(view(enc)).encode() != enc) ev_roundtrip_ok = false;
  }
  CHECK_MSG(ev_roundtrip_ok, "decode(encode(evidence)) re-encodes identically");

  std::printf("== consensus-data round-trip property (5k random values) ==\n");
  bool cd_roundtrip_ok = true;
  for (int i = 0; i < 5000; ++i) {
    consensus::ConsensusData cd;
    for (auto& x : cd.validator_set_hash) x = static_cast<std::uint8_t>(rng.next());
    if (rng.next() & 1) {  // half the time, carry a last commit
      consensus::CommitCertificate c;
      c.height = rng.next() >> 1;
      c.round = static_cast<std::uint32_t>(rng.next());
      for (auto& x : c.block_id) x = static_cast<std::uint8_t>(rng.next());
      const std::uint32_t n = rng.below(5);
      for (std::uint32_t s = 0; s < n; ++s) c.signers.push_back(s);  // ascending
      c.agg_signature = random_bytes(rng, rng.below(100));
      cd.last_commit = c;
    }
    Bytes enc = cd.encode();
    if (consensus::ConsensusData::decode_all(view(enc)).encode() != enc) cd_roundtrip_ok = false;
  }
  CHECK_MSG(cd_roundtrip_ok, "decode(encode(consensus_data)) re-encodes identically");

  return bctest::summary();
}
