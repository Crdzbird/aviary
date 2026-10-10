#include "aviary_c.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <vector>

#include "aviary/consensus.hpp"
#include "aviary/crypto.hpp"
#include "aviary/interop/bitcoin.hpp"
#include "aviary/interop/abi.hpp"
#include "aviary/interop/evm.hpp"
#include "aviary/interop/solana.hpp"
#include "aviary/keyring.hpp"
#include "aviary/assurance.hpp"
#include "aviary/chain_assembler.hpp"
#include "aviary/chain_blueprint.hpp"
#include "aviary/foreign_engine.hpp"
#include "aviary/account_state.hpp"
#include "aviary/bft_engine.hpp"
#include "aviary/subchain.hpp"
#include "aviary/light_client.hpp"
#include "aviary/chain_tree.hpp"
#include "aviary/genesis.hpp"
#include "aviary/pos_engine.hpp"
#include "aviary/pow_engine.hpp"
#include "aviary/vrf_pos.hpp"
#include "aviary/credential_gate.hpp"
#include "aviary/node/demo_node.hpp"
#include "aviary/proofs.hpp"
#include "aviary/sealed_mempool.hpp"
#include "aviary/threshold.hpp"
#include "aviary/codec.hpp"
#include "aviary/types.hpp"
#include "aviary/version.hpp"

namespace {

// Duplicate a std::string into a malloc'd C string (caller frees via
// aviary_str_free). Returns nullptr on allocation failure.
char* dup_cstr(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (p == nullptr) return nullptr;
  std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

AviaryBufferResult error_result(const std::string& msg) {
  return AviaryBufferResult{{nullptr, 0}, dup_cstr(msg)};
}

// Copy raw bytes into a malloc'd buffer result.
AviaryBufferResult bytes_result(const std::uint8_t* data, std::size_t len) {
  auto* out = static_cast<std::uint8_t*>(std::malloc(len == 0 ? 1 : len));
  if (out == nullptr) return error_result("out of memory");
  std::memcpy(out, data, len);
  return AviaryBufferResult{{out, len}, nullptr};
}
AviaryBufferResult bytes_result(const std::vector<std::uint8_t>& b) {
  return bytes_result(b.data(), b.size());
}
AviaryBufferResult string_result(const std::string& s) {
  return bytes_result(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

aviary::crypto::Hash to_hash32(const std::uint8_t* p) {
  aviary::crypto::Hash h{};
  std::memcpy(h.data(), p, 32);
  return h;
}

aviary::node::DemoNode* as_node(AviaryNode n) {
  return static_cast<aviary::node::DemoNode*>(n);
}

aviary::custody::KeyStore* as_ks(AviaryKeyStore k) {
  return static_cast<aviary::custody::KeyStore*>(k);
}

// Pack a {public_key, secret} pair as the self-describing account blob
// [pub_len:4 BE][public][secret] — the same layout new_account_hybrid emits.
AviaryBufferResult account_blob(const aviary::crypto::Bytes& pub,
                                      const aviary::crypto::Bytes& sec) {
  std::vector<std::uint8_t> out;
  const std::uint32_t pl = static_cast<std::uint32_t>(pub.size());
  out.push_back(static_cast<std::uint8_t>((pl >> 24) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((pl >> 16) & 0xFF));
  out.push_back(static_cast<std::uint8_t>((pl >> 8) & 0xFF));
  out.push_back(static_cast<std::uint8_t>(pl & 0xFF));
  out.insert(out.end(), pub.begin(), pub.end());
  out.insert(out.end(), sec.begin(), sec.end());
  return bytes_result(out);
}

aviary::crypto::ByteView cbv(const std::uint8_t* p, std::size_t n) {
  return aviary::crypto::ByteView(p, n);
}

namespace cg = aviary::consensus;

cg::CredentialGate* as_gate(AviaryCredentialGate g) {
  return static_cast<cg::CredentialGate*>(g);
}

// Credential blob = [sig_len:4][sig]([attr_count:4]([attr_len:4][attr])×).
// Holder-secret (carries member_secret + hidden attributes).
std::vector<std::uint8_t> encode_credential(const cg::Credential& c) {
  aviary::codec::CodecWriter w;
  w.write_bytes(cbv(c.signature.data(), c.signature.size()));
  w.write_list(c.attrs,
               [](aviary::codec::CodecWriter& ww, const aviary::crypto::Bytes& a) {
                 ww.write_bytes(cbv(a.data(), a.size()));
               });
  return w.take();
}
cg::Credential decode_credential(const std::uint8_t* p, std::size_t n) {
  aviary::codec::CodecReader r(cbv(p, n));
  cg::Credential c;
  c.signature = r.read_bytes();
  c.attrs = r.read_list<aviary::crypto::Bytes>(
      [](aviary::codec::CodecReader& rr) { return rr.read_bytes(); });
  r.assert_fully_consumed();
  return c;
}

// ── Option B blob (de)serialisers ──────────────────────────────────────────
using CW = aviary::codec::CodecWriter;
using CR = aviary::codec::CodecReader;

// membership-commit = [commitment][nym_seed][secret_prover_blind]
std::vector<std::uint8_t> encode_membership_commit(const cg::MembershipCommitment& m) {
  CW w;
  w.write_bytes(cbv(m.commitment.data(), m.commitment.size()));
  w.write_bytes(cbv(m.nym_seed.data(), m.nym_seed.size()));
  w.write_bytes(cbv(m.secret_prover_blind.data(), m.secret_prover_blind.size()));
  return w.take();
}
cg::MembershipCommitment decode_membership_commit(const std::uint8_t* p, std::size_t n) {
  CR r(cbv(p, n));
  cg::MembershipCommitment m;
  m.commitment = r.read_bytes();
  m.nym_seed = r.read_bytes();
  m.secret_prover_blind = r.read_bytes();
  r.assert_fully_consumed();
  return m;
}

// blind-issuance = [sig][entropy][tier u8][not_after u64][issuer_epoch u32]
std::vector<std::uint8_t> encode_blind_issuance(const cg::BlindIssuance& b) {
  CW w;
  w.write_bytes(cbv(b.signature.data(), b.signature.size()));
  w.write_bytes(cbv(b.entropy.data(), b.entropy.size()));
  w.write_u8(b.tier);
  w.write_u64(b.not_after);
  w.write_u32(b.issuer_epoch);
  return w.take();
}
cg::BlindIssuance decode_blind_issuance(const std::uint8_t* p, std::size_t n) {
  CR r(cbv(p, n));
  cg::BlindIssuance b;
  b.signature = r.read_bytes();
  b.entropy = r.read_bytes();
  b.tier = r.read_u8();
  b.not_after = r.read_u64();
  b.issuer_epoch = r.read_u32();
  r.assert_fully_consumed();
  return b;
}

// pseudonymous credential = [sig][tier u8][not_after u64][issuer_epoch u32][nym_secrets…][spb]
std::vector<std::uint8_t> encode_pseudonymous_credential(const cg::PseudonymousCredential& c) {
  CW w;
  w.write_bytes(cbv(c.signature.data(), c.signature.size()));
  w.write_u8(c.tier);
  w.write_u64(c.not_after);
  w.write_u32(c.issuer_epoch);
  w.write_list(c.nym_secrets, [](CW& ww, const aviary::crypto::Bytes& s) {
    ww.write_bytes(cbv(s.data(), s.size()));
  });
  w.write_bytes(cbv(c.secret_prover_blind.data(), c.secret_prover_blind.size()));
  return w.take();
}
cg::PseudonymousCredential decode_pseudonymous_credential(const std::uint8_t* p, std::size_t n) {
  CR r(cbv(p, n));
  cg::PseudonymousCredential c;
  c.signature = r.read_bytes();
  c.tier = r.read_u8();
  c.not_after = r.read_u64();
  c.issuer_epoch = r.read_u32();
  c.nym_secrets = r.read_list<aviary::crypto::Bytes>([](CR& rr) { return rr.read_bytes(); });
  c.secret_prover_blind = r.read_bytes();
  r.assert_fully_consumed();
  return c;
}


// The chain handle: the assembled chain plus what this validator needs to
// certify its own BFT blocks (its index + BLS key).
struct ChainBox {
  aviary::node::AssembledChain chain;
  std::uint32_t me;
  aviary::crypto::KeyPair key;
};
ChainBox* as_box(AviaryChain c) { return static_cast<ChainBox*>(c); }
aviary::node::AssembledChain* as_chain(AviaryChain c) { return &as_box(c)->chain; }
// [count u32]([len u32][bytes])× → the items.
std::vector<aviary::crypto::Bytes> decode_blob_list(aviary::codec::ByteView v) {
  aviary::codec::CodecReader r(v);
  auto items = r.read_list<aviary::crypto::Bytes>([](aviary::codec::CodecReader& rr) { return rr.read_bytes(); });
  r.assert_fully_consumed();
  return items;
}
std::vector<aviary::Anchor> decode_anchors(aviary::codec::ByteView v) {
  std::vector<aviary::Anchor> out;
  for (const auto& b : decode_blob_list(v)) out.push_back(aviary::Anchor::decode_all(cbv(b.data(), b.size())));
  return out;
}
// [count u32]([len u32][id])×
std::vector<std::uint8_t> encode_ids(const std::vector<std::string>& ids) {
  aviary::codec::CodecWriter w;
  w.write_list(ids, [](aviary::codec::CodecWriter& ww, const std::string& id) {
    ww.write_bytes(cbv(reinterpret_cast<const std::uint8_t*>(id.data()), id.size()));
  });
  return w.take();
}
}  // namespace

extern "C" {

int aviary_init(void) {
  try {
    aviary::init();
    return 0;
  } catch (...) {
    return 1;
  }
}

const char* aviary_version(void) {
  // Static storage so the returned pointer outlives the call.
  static const std::string v = aviary::version();
  return v.c_str();
}

const char* aviary_crypto_backend_version(void) {
  static const std::string v = aviary::crypto_backend_version();
  return v.c_str();
}

AviaryBufferResult aviary_hash(const uint8_t* msg, size_t len) {
  try {
    const aviary::crypto::Hash h = aviary::crypto::blake3_256(msg, len);
    auto* out = static_cast<uint8_t*>(std::malloc(h.size()));
    if (out == nullptr) return error_result("out of memory");
    std::memcpy(out, h.data(), h.size());
    return AviaryBufferResult{{out, h.size()}, nullptr};
  } catch (const std::exception& e) {
    return error_result(e.what());
  } catch (...) {
    return error_result("unknown error");
  }
}

namespace {
// Copy a 32-byte hash into a fresh malloc'd buffer result.
AviaryBufferResult hash_result(const aviary::crypto::Hash& h) {
  auto* out = static_cast<uint8_t*>(std::malloc(h.size()));
  if (out == nullptr) return error_result("out of memory");
  std::memcpy(out, h.data(), h.size());
  return AviaryBufferResult{{out, h.size()}, nullptr};
}

aviary::Bytes to_bytes(const uint8_t* p, size_t n) {
  return p == nullptr ? aviary::Bytes{} : aviary::Bytes(p, p + n);
}
}  // namespace

AviaryBufferResult aviary_txid(uint8_t version, uint64_t chain_id,
                                           const uint8_t* body, size_t body_len,
                                           const uint8_t* author, size_t author_len,
                                           const uint8_t* signature, size_t sig_len) {
  try {
    aviary::Transaction tx;
    tx.version = version;
    tx.chain_id = chain_id;
    tx.body = to_bytes(body, body_len);
    tx.author = to_bytes(author, author_len);
    tx.signature = to_bytes(signature, sig_len);
    return hash_result(tx.txid());
  } catch (const std::exception& e) {
    return error_result(e.what());
  } catch (...) {
    return error_result("unknown error");
  }
}

AviaryBufferResult aviary_header_hash(
    uint8_t version, uint64_t chain_id, uint64_t height,
    const uint8_t* parent_hash, uint64_t timestamp,
    const uint8_t* tx_root, const uint8_t* state_root,
    const uint8_t* consensus_data, size_t consensus_len) {
  try {
    aviary::BlockHeader h;
    h.version = version;
    h.chain_id = chain_id;
    h.height = height;
    h.timestamp = timestamp;
    std::memcpy(h.parent_hash.data(), parent_hash, 32);
    std::memcpy(h.tx_root.data(), tx_root, 32);
    std::memcpy(h.state_root.data(), state_root, 32);
    h.consensus_data = to_bytes(consensus_data, consensus_len);
    return hash_result(h.hash());
  } catch (const std::exception& e) {
    return error_result(e.what());
  } catch (...) {
    return error_result("unknown error");
  }
}

namespace {
aviary::codec::ByteView cview(const uint8_t* p, size_t n) {
  return aviary::codec::ByteView(p, n);
}
aviary::crypto::Hash hash32(const uint8_t* p) {
  aviary::crypto::Hash h{};
  std::memcpy(h.data(), p, 32);
  return h;
}
}  // namespace

int aviary_verify_state_proof(const uint8_t* state_root, const uint8_t* proof,
                                    size_t proof_len) {
  try {
    auto p = aviary::StateProof::decode_all(cview(proof, proof_len));
    return aviary::verify_state_proof(hash32(state_root), p) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int aviary_verify_tx_inclusion(const uint8_t* tx_root, const uint8_t* proof,
                                     size_t proof_len) {
  try {
    auto p = aviary::TxInclusionProof::decode_all(cview(proof, proof_len));
    return aviary::verify_tx_inclusion(hash32(tx_root), p) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int aviary_verify_commit(uint64_t chain_id, const uint8_t* validator_set,
                               size_t vset_len, const uint8_t* cert, size_t cert_len) {
  try {
    auto vset = aviary::consensus::ValidatorSet::decode_all(cview(validator_set, vset_len));
    auto c = aviary::consensus::CommitCertificate::decode_all(cview(cert, cert_len));
    return aviary::consensus::verify_commit(chain_id, vset, c).ok ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

// ── Demo node ─────────────────────────────────────────────────────────────────

AviaryNode aviary_node_new(uint64_t chain_id) {
  try {
    return new aviary::node::DemoNode(chain_id);
  } catch (...) {
    return nullptr;
  }
}

void aviary_node_free(AviaryNode node) { delete as_node(node); }

AviaryBufferResult aviary_node_new_account(AviaryNode node) {
  try {
    auto kp = as_node(node)->new_account();  // secret 64, public 32
    return bytes_result(kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_account_address(const uint8_t* public_key, size_t pk_len) {
  try {
    auto h = aviary::crypto::address_of(aviary::codec::ByteView(public_key, pk_len));
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int aviary_node_credit(AviaryNode node, const uint8_t* address, uint64_t amount) {
  try {
    as_node(node)->credit(to_hash32(address), static_cast<aviary::codec::u128>(amount));
    return AVIARY_OK;
  } catch (const aviary::node::GenesisSealedError&) {
    return AVIARY_ERR_GENESIS_SEALED;
  } catch (...) {
    return AVIARY_ERR;
  }
}

AviaryBufferResult aviary_node_submit_transfer(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce) {
  try {
    auto txid = as_node(node)->submit_transfer(
        aviary::codec::ByteView(secret, sk_len), to_hash32(to_address),
        static_cast<aviary::codec::u128>(amount), nonce);
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_node_submit_transfer_fee(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce, uint64_t fee) {
  try {
    auto txid = as_node(node)->submit_transfer_fee(
        aviary::codec::ByteView(secret, sk_len), to_hash32(to_address),
        static_cast<aviary::codec::u128>(amount), nonce,
        static_cast<aviary::codec::u128>(fee));
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_node_new_account_hybrid(AviaryNode node) {
  try {
    auto kp = as_node(node)->new_account_hybrid();
    std::vector<std::uint8_t> out;
    const std::uint32_t pl = static_cast<std::uint32_t>(kp.public_key.size());
    out.push_back(static_cast<std::uint8_t>((pl >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((pl >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((pl >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(pl & 0xFF));
    out.insert(out.end(), kp.public_key.begin(), kp.public_key.end());
    out.insert(out.end(), kp.secret_key.begin(), kp.secret_key.end());
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_node_submit_transfer_hybrid(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* public_key, size_t pk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce) {
  try {
    auto txid = as_node(node)->submit_transfer_hybrid(
        aviary::codec::ByteView(secret, sk_len),
        aviary::codec::ByteView(public_key, pk_len), to_hash32(to_address),
        static_cast<aviary::codec::u128>(amount), nonce);
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int aviary_node_produce_block(AviaryNode node, uint64_t timestamp, uint64_t* out_height) {
  try {
    uint64_t h = as_node(node)->produce_block(timestamp);
    if (out_height) *out_height = h;
    return 0;
  } catch (...) {
    return 1;
  }
}

uint64_t aviary_node_height(AviaryNode node) {
  try { return as_node(node)->height(); } catch (...) { return 0; }
}
uint64_t aviary_node_balance(AviaryNode node, const uint8_t* address) {
  try { return static_cast<uint64_t>(as_node(node)->balance(to_hash32(address))); }
  catch (...) { return 0; }
}
uint64_t aviary_node_nonce(AviaryNode node, const uint8_t* address) {
  try { return as_node(node)->nonce(to_hash32(address)); } catch (...) { return 0; }
}
uint64_t aviary_node_mempool_size(AviaryNode node) {
  try { return as_node(node)->mempool_size(); } catch (...) { return 0; }
}

AviaryBufferResult aviary_node_state_root(AviaryNode node) {
  try {
    auto h = as_node(node)->state_root();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_node_drain_events(AviaryNode node) {
  try { return string_result(as_node(node)->drain_events_json()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}
void aviary_node_set_event_hook(AviaryNode node, AviaryEventFn fn, void* ctx) {
  if (!node) return;
  if (!fn) {
    as_node(node)->set_event_hook(nullptr);
    return;
  }
  as_node(node)->set_event_hook(
      [fn, ctx](const std::string& json) { fn(ctx, json.data(), json.size()); });
}
AviaryBufferResult aviary_node_metrics(AviaryNode node) {
  try { return string_result(as_node(node)->metrics_text()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_node_prove_account(AviaryNode node, const uint8_t* address) {
  try { return bytes_result(as_node(node)->prove_account(to_hash32(address)).encode()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}

// ── Custodial key store ───────────────────────────────────────────────────────

AviaryKeyStore aviary_keystore_new(const char* passphrase) {
  try {
    return new aviary::custody::KeyStore(
        aviary::custody::KeyStore::create_with_passphrase(passphrase ? passphrase : ""));
  } catch (...) {
    return nullptr;
  }
}

int aviary_keystore_add_account(AviaryKeyStore ks, const uint8_t* public_key,
                                      size_t pk_len, const uint8_t* secret, size_t sk_len,
                                      uint8_t* out_address) {
  try {
    aviary::crypto::Hash addr = as_ks(ks)->add_account(
        aviary::crypto::ByteView(public_key, pk_len),
        aviary::crypto::ByteView(secret, sk_len));
    std::memcpy(out_address, addr.data(), 32);
    return 0;
  } catch (...) {
    return 1;
  }
}

AviaryBufferResult aviary_keystore_reveal(AviaryKeyStore ks,
                                                      const uint8_t* address) {
  try {
    return bytes_result(as_ks(ks)->reveal_secret(to_hash32(address)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_keystore_serialise(AviaryKeyStore ks) {
  try {
    return bytes_result(as_ks(ks)->serialise());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryKeyStore aviary_keystore_load(const uint8_t* blob, size_t len) {
  try {
    return new aviary::custody::KeyStore(
        aviary::custody::KeyStore::load(aviary::crypto::ByteView(blob, len)));
  } catch (...) {
    return nullptr;
  }
}

int aviary_keystore_unlock(AviaryKeyStore ks, const char* passphrase) {
  try {
    as_ks(ks)->unlock_with_passphrase(passphrase ? passphrase : "");
    return 0;
  } catch (...) {
    return 1;
  }
}

int aviary_keystore_unlocked(AviaryKeyStore ks) {
  return as_ks(ks)->unlocked() ? 1 : 0;
}

void aviary_keystore_free(AviaryKeyStore ks) { delete as_ks(ks); }

// ── Portable account export (tiered encrypted backup) ─────────────────────────

AviaryBufferResult aviary_hybrid_kem_keygen(void) {
  try {
    auto kp = aviary::crypto::hybrid_kem_keygen();
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_seal_passphrase(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const char* passphrase, uint64_t ops, size_t mem) {
  try {
    aviary::custody::PortableAccount acct{
        aviary::crypto::Bytes(public_key, public_key + pk_len),
        aviary::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_passphrase(passphrase ? passphrase : "", ops, mem));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_open_passphrase(
    const uint8_t* envelope, size_t env_len, const char* passphrase) {
  try {
    auto a = aviary::custody::PortableAccount::open_passphrase(cbv(envelope, env_len),
                                                                    passphrase ? passphrase : "");
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_seal_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len) {
  try {
    aviary::custody::PortableAccount acct{
        aviary::crypto::Bytes(public_key, public_key + pk_len),
        aviary::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_to(cbv(recipient_kem_public, rk_len)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_open_to(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len) {
  try {
    auto a = aviary::custody::PortableAccount::open_to(cbv(envelope, env_len),
                                                            cbv(recipient_kem_secret, rk_len));
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_seal_signed_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len,
    const uint8_t* sender_sig_secret, size_t ss_len) {
  try {
    aviary::custody::PortableAccount acct{
        aviary::crypto::Bytes(public_key, public_key + pk_len),
        aviary::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_signed_to(cbv(recipient_kem_public, rk_len),
                                            cbv(sender_sig_secret, ss_len)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_portable_open_signed_from(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len,
    const uint8_t* sender_sig_public, size_t sp_len) {
  try {
    auto a = aviary::custody::PortableAccount::open_signed_from(
        cbv(envelope, env_len), cbv(recipient_kem_secret, rk_len), cbv(sender_sig_public, sp_len));
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

// ── Threshold accounts (FROST t-of-n) ─────────────────────────────────────────

namespace {
namespace th = aviary::custody;

void put_u16(std::vector<std::uint8_t>& v, std::uint16_t x) {
  v.push_back(static_cast<std::uint8_t>(x >> 8));
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}
void put_u32(std::vector<std::uint8_t>& v, std::uint32_t x) {
  v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xFF));
  v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
  v.push_back(static_cast<std::uint8_t>(x & 0xFF));
}
std::uint16_t rd_u16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

th::SigningCeremony* as_ceremony(AviaryCeremony c) {
  return static_cast<th::SigningCeremony*>(c);
}
}  // namespace

AviaryBufferResult aviary_threshold_create(uint16_t n, uint16_t t) {
  try {
    std::vector<th::ShareCard> cards = th::ThresholdAccount::create(n, t);
    std::vector<std::uint8_t> out;
    put_u16(out, static_cast<std::uint16_t>(cards.size()));
    for (const auto& card : cards) {
      const aviary::crypto::Bytes enc = card.encode();
      put_u32(out, static_cast<std::uint32_t>(enc.size()));
      out.insert(out.end(), enc.begin(), enc.end());
    }
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_threshold_share_info(const uint8_t* card, size_t card_len) {
  try {
    th::ShareCard c = th::ShareCard::decode(cbv(card, card_len));
    std::vector<std::uint8_t> out;
    put_u16(out, c.index);
    out.insert(out.end(), c.group_public_key.begin(), c.group_public_key.end());
    out.insert(out.end(), c.public_share.begin(), c.public_share.end());
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_threshold_share_seal(const uint8_t* card, size_t card_len,
                                                           const char* passphrase, uint64_t ops,
                                                           size_t mem) {
  try {
    th::ShareCard c = th::ShareCard::decode(cbv(card, card_len));
    return bytes_result(c.seal(passphrase ? passphrase : "", ops, mem));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_threshold_share_unseal(const uint8_t* envelope, size_t env_len,
                                                             const char* passphrase) {
  try {
    th::ShareCard c = th::ShareCard::unseal(cbv(envelope, env_len), passphrase ? passphrase : "");
    return bytes_result(c.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_threshold_commit(const uint8_t* card, size_t card_len) {
  try {
    th::ShareCard c = th::ShareCard::decode(cbv(card, card_len));
    auto [nonces, commitment] = th::SigningCeremony::commit(c);
    std::vector<std::uint8_t> out;
    out.insert(out.end(), nonces.hiding.begin(), nonces.hiding.end());   // [0..32)
    out.insert(out.end(), nonces.binding.begin(), nonces.binding.end()); // [32..64)
    put_u16(out, commitment.id);                                         // commitment: [64..66)
    out.insert(out.end(), commitment.hiding.begin(), commitment.hiding.end());
    out.insert(out.end(), commitment.binding.begin(), commitment.binding.end());
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryCeremony aviary_threshold_ceremony_new(const uint8_t* group_pk32,
                                                         const uint8_t* message, size_t msg_len,
                                                         const uint8_t* commitments,
                                                         size_t commits_len) {
  try {
    if (commits_len % 66 != 0) return nullptr;  // each commitment is id(2)+32+32
    std::vector<th::SigningCeremony::Commitment> roster;
    for (size_t off = 0; off + 66 <= commits_len; off += 66) {
      th::SigningCeremony::Commitment cm;
      cm.id = rd_u16(commitments + off);
      cm.hiding.assign(commitments + off + 2, commitments + off + 34);
      cm.binding.assign(commitments + off + 34, commitments + off + 66);
      roster.push_back(std::move(cm));
    }
    return new th::SigningCeremony(aviary::crypto::Bytes(group_pk32, group_pk32 + 32),
                                   aviary::crypto::Bytes(message, message + msg_len),
                                   std::move(roster));
  } catch (...) {
    return nullptr;
  }
}

void aviary_threshold_ceremony_free(AviaryCeremony c) { delete as_ceremony(c); }

AviaryBufferResult aviary_threshold_sign(AviaryCeremony c, const uint8_t* card,
                                                     size_t card_len, const uint8_t* nonces64) {
  try {
    th::ShareCard share = th::ShareCard::decode(cbv(card, card_len));
    th::SigningCeremony::Nonces nonces{aviary::crypto::Bytes(nonces64, nonces64 + 32),
                                       aviary::crypto::Bytes(nonces64 + 32, nonces64 + 64)};
    return bytes_result(as_ceremony(c)->sign(share, nonces));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int aviary_threshold_verify_share(AviaryCeremony c, uint16_t id, const uint8_t* public32,
                                        const uint8_t* sig_share32) {
  try {
    return as_ceremony(c)->verify_share(id, aviary::crypto::Bytes(public32, public32 + 32),
                                        aviary::crypto::Bytes(sig_share32, sig_share32 + 32))
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

AviaryBufferResult aviary_threshold_aggregate(AviaryCeremony c,
                                                          const uint16_t* ids,
                                                          const uint8_t* sig_shares, size_t count) {
  try {
    th::SigningCeremony* cer = as_ceremony(c);
    // Reorder the caller's (id → share) pairs into the ceremony's roster order,
    // which is what SigningCeremony::aggregate expects.
    std::vector<aviary::crypto::Bytes> ordered;
    for (const auto& cm : cer->roster()) {
      bool found = false;
      for (size_t i = 0; i < count; ++i) {
        if (ids[i] == cm.id) {
          ordered.emplace_back(sig_shares + i * 32, sig_shares + i * 32 + 32);
          found = true;
          break;
        }
      }
      if (!found) return error_result("threshold: missing signature share for a roster participant");
    }
    return bytes_result(cer->aggregate(ordered));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

// ── AssuranceProfile ──────────────────────────────────────────────────────────

AviaryBufferResult aviary_assurance_describe(uint16_t level) {
  try {
    aviary::AssuranceProfile p = [level]() {
      switch (level) {
        case 1: return aviary::AssuranceProfile::standard();
        case 2: return aviary::AssuranceProfile::hardened();
        case 3: return aviary::AssuranceProfile::fortress();
        default: throw std::runtime_error("assurance: unknown profile level");
      }
    }();
    std::vector<std::uint8_t> out;
    out.push_back(p.transport() == aviary::AssuranceProfile::Transport::kRatcheted ? 1 : 0);
    out.push_back(p.consensus() == aviary::AssuranceProfile::Consensus::kVrf ? 1 : 0);
    out.push_back(p.threshold_custody() ? 1 : 0);
    out.push_back(p.sealed_mempool() ? 1 : 0);
    const auto algos = p.required_sig_algos();
    out.push_back(static_cast<std::uint8_t>(algos.size()));
    out.insert(out.end(), algos.begin(), algos.end());
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

// ── Transaction encoding ──────────────────────────────────────────────────────

AviaryBufferResult aviary_tx_encode(uint8_t version, uint64_t chain_id, uint8_t sig_algo,
                                                uint64_t fee, const uint8_t* body, size_t body_len,
                                                const uint8_t* author, size_t author_len,
                                                const uint8_t* signature, size_t sig_len) {
  try {
    aviary::Transaction tx;
    tx.version = version;
    tx.chain_id = chain_id;
    tx.sig_algo = sig_algo;
    tx.fee = static_cast<aviary::codec::u128>(fee);
    tx.body.assign(body, body + body_len);
    tx.author.assign(author, author + author_len);
    tx.signature.assign(signature, signature + sig_len);
    return bytes_result(tx.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

// ── SealedMempool ─────────────────────────────────────────────────────────────

namespace {
aviary::p2p::SealedMempool* as_pool(AviarySealedMempool m) {
  return static_cast<aviary::p2p::SealedMempool*>(m);
}
}  // namespace

AviaryBufferResult aviary_sealed_tx_seal(const uint8_t* tx_encoded, size_t tx_len,
                                                     const uint8_t* proposer_kem_public,
                                                     size_t pk_len) {
  try {
    aviary::Transaction tx = aviary::Transaction::decode_all(cbv(tx_encoded, tx_len));
    aviary::p2p::SealedTx sealed =
        aviary::p2p::SealedTx::seal(tx, cbv(proposer_kem_public, pk_len));
    return bytes_result(sealed.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviarySealedMempool aviary_sealed_mempool_new(size_t capacity) {
  try {
    return new aviary::p2p::SealedMempool(capacity);
  } catch (...) {
    return nullptr;
  }
}

int aviary_sealed_mempool_add(AviarySealedMempool m, const uint8_t* sealed_tx,
                                    size_t len) {
  try {
    return as_pool(m)->add(aviary::p2p::SealedTx::decode(cbv(sealed_tx, len))) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

size_t aviary_sealed_mempool_size(AviarySealedMempool m) { return as_pool(m)->size(); }

AviaryBufferResult aviary_sealed_mempool_reveal(AviarySealedMempool m,
                                                            const uint8_t* proposer_kem_secret,
                                                            size_t sk_len, size_t max) {
  try {
    std::vector<aviary::Transaction> txs =
        as_pool(m)->reveal(cbv(proposer_kem_secret, sk_len), max);
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(txs.size()));
    for (const auto& tx : txs) {
      const aviary::crypto::Bytes enc = tx.encode();
      put_u32(out, static_cast<std::uint32_t>(enc.size()));
      out.insert(out.end(), enc.begin(), enc.end());
    }
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

void aviary_sealed_mempool_free(AviarySealedMempool m) { delete as_pool(m); }

// ── CredentialGate (anonymous membership admission, Option A) ──────────────────

AviaryBufferResult aviary_bbs_keygen(const uint8_t* key_material, size_t km_len) {
  try {
    aviary::crypto::KeyPair kp = aviary::crypto::bbs_keygen(cbv(key_material, km_len));
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_policy_encode(uint64_t chain_id,
                                                               const uint8_t* issuer_pk,
                                                               size_t pk_len, const uint8_t* header,
                                                               size_t header_len,
                                                               uint32_t attribute_count,
                                                               uint8_t min_tier) {
  try {
    cg::CredentialPolicy p;
    p.chain_id = chain_id;
    p.issuer_public_key.assign(issuer_pk, issuer_pk + pk_len);
    p.header.assign(header, header + header_len);
    p.attribute_count = attribute_count;
    p.min_tier = min_tier;
    return bytes_result(p.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_policy_hash(const uint8_t* policy,
                                                             size_t policy_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    aviary::crypto::Hash h = p.hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_issue(const uint8_t* policy, size_t policy_len,
                                                       const uint8_t* issuer_sk, size_t sk_len,
                                                       const uint8_t* member_secret, size_t ms_len,
                                                       uint8_t tier, uint64_t not_after,
                                                       uint32_t issuer_epoch) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    cg::Credential c = cg::issue_credential(p, cbv(issuer_sk, sk_len), cbv(member_secret, ms_len),
                                            tier, not_after, issuer_epoch);
    return bytes_result(encode_credential(c));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_present(const uint8_t* policy, size_t policy_len,
                                                         const uint8_t* credential, size_t cred_len,
                                                         uint32_t epoch, const char* scope,
                                                         const uint8_t* challenge,
                                                         size_t challenge_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    cg::Credential c = decode_credential(credential, cred_len);
    std::vector<std::uint8_t> pres = cg::CredentialGate::present(
        p, c, epoch, scope ? scope : "", cbv(challenge, challenge_len));
    return bytes_result(pres);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryCredentialGate aviary_credential_gate_new(const uint8_t* policy,
                                                            size_t policy_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    return new cg::CredentialGate(std::move(p));
  } catch (...) {
    return nullptr;
  }
}

void aviary_credential_gate_revoke_epoch(AviaryCredentialGate g, uint32_t epoch) {
  if (g) as_gate(g)->revoke_issuer_epoch(epoch);
}

int aviary_credential_gate_admit(AviaryCredentialGate g, uint32_t epoch,
                                       const char* scope, const uint8_t* challenge,
                                       size_t challenge_len, const uint8_t* presentation,
                                       size_t presentation_len, uint64_t now) {
  if (g == nullptr) return 0;
  return as_gate(g)->admit(epoch, scope ? scope : "", cbv(challenge, challenge_len),
                           cbv(presentation, presentation_len), now)
             ? 1
             : 0;
}

void aviary_credential_gate_free(AviaryCredentialGate g) { delete as_gate(g); }

// ── CredentialGate Option B — pseudonymous ────────────────────────────────────

AviaryBufferResult aviary_credential_policy_encode_ex(uint64_t chain_id,
                                                                  const uint8_t* issuer_pk,
                                                                  size_t pk_len,
                                                                  const uint8_t* header,
                                                                  size_t header_len,
                                                                  uint32_t attribute_count,
                                                                  uint8_t min_tier, uint8_t mode) {
  try {
    cg::CredentialPolicy p;
    p.chain_id = chain_id;
    p.issuer_public_key.assign(issuer_pk, issuer_pk + pk_len);
    p.header.assign(header, header + header_len);
    p.attribute_count = attribute_count;
    p.min_tier = min_tier;
    p.mode = static_cast<cg::CredentialMode>(mode);
    return bytes_result(p.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_membership_commit(const uint8_t* policy, size_t policy_len,
                                                        const uint8_t* member_secret,
                                                        size_t ms_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    return bytes_result(encode_membership_commit(cg::membership_commit(p, cbv(member_secret, ms_len))));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_blind_issue(const uint8_t* policy, size_t policy_len,
                                                  const uint8_t* issuer_sk, size_t sk_len,
                                                  const uint8_t* commitment, size_t commitment_len,
                                                  uint8_t tier, uint64_t not_after,
                                                  uint32_t issuer_epoch) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    cg::BlindIssuance b = cg::blind_issue(p, cbv(issuer_sk, sk_len), cbv(commitment, commitment_len),
                                          tier, not_after, issuer_epoch);
    return bytes_result(encode_blind_issuance(b));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_finalize_membership(const uint8_t* membership_commit,
                                                          size_t mc_len,
                                                          const uint8_t* blind_issuance,
                                                          size_t bi_len) {
  try {
    cg::MembershipCommitment m = decode_membership_commit(membership_commit, mc_len);
    cg::BlindIssuance b = decode_blind_issuance(blind_issuance, bi_len);
    return bytes_result(encode_pseudonymous_credential(cg::finalize_membership(m, b)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_present_pseudonymous(
    const uint8_t* policy, size_t policy_len, const uint8_t* credential, size_t cred_len,
    uint32_t epoch, const char* scope, const uint8_t* challenge, size_t challenge_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    cg::PseudonymousCredential c = decode_pseudonymous_credential(credential, cred_len);
    std::vector<std::uint8_t> pres = cg::CredentialGate::present_pseudonymous(
        p, c, epoch, scope ? scope : "", cbv(challenge, challenge_len));
    return bytes_result(pres);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_credential_gate_admit_pseudonymous(
    AviaryCredentialGate g, uint32_t epoch, const char* scope, const uint8_t* challenge,
    size_t challenge_len, const uint8_t* presentation, size_t presentation_len, uint64_t now) {
  if (g == nullptr) return error_result("null gate");
  std::optional<aviary::crypto::Bytes> nym =
      as_gate(g)->admit_pseudonymous(epoch, scope ? scope : "", cbv(challenge, challenge_len),
                                     cbv(presentation, presentation_len), now);
  if (!nym) {
    const std::uint8_t empty = 0;
    return bytes_result(&empty, 0);  // reject → empty buffer (len 0), no error
  }
  return bytes_result(*nym);
}

void aviary_credential_gate_revoke_nym(AviaryCredentialGate g, const uint8_t* nym,
                                             size_t nym_len) {
  if (g) as_gate(g)->revoke_nym(cbv(nym, nym_len));
}

int aviary_credential_gate_note_admission(AviaryCredentialGate g, uint32_t epoch,
                                                const char* scope, const uint8_t* nym,
                                                size_t nym_len) {
  if (g == nullptr) return 0;
  return as_gate(g)->note_admission(epoch, scope ? scope : "", cbv(nym, nym_len)) ? 1 : 0;
}


// ── ChainBlueprint + ChainAssembler (composable chains) ───────────────────────

AviaryBufferResult aviary_bls_keygen(const uint8_t* ikm, size_t ikm_len) {
  try {
    aviary::crypto::KeyPair kp = aviary::crypto::bls_keygen_from_ikm(cbv(ikm, ikm_len));
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_blueprint_encode(const char* consensus_id,
                                                       const char* state_model_id,
                                                       const char* proposer_id,
                                                       const uint8_t* params, size_t params_len) {
  try {
    aviary::ChainBlueprint b;
    b.consensus_id = consensus_id ? consensus_id : "";
    b.state_model_id = state_model_id ? state_model_id : "";
    b.proposer_id = proposer_id ? proposer_id : "";
    if (params && params_len) b.component_params.assign(params, params + params_len);
    return bytes_result(b.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_blueprint_hash(const uint8_t* blueprint, size_t blueprint_len) {
  try {
    aviary::crypto::Hash h = aviary::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len)).hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_registry_ids(uint8_t kind) {
  using R = aviary::node::ComponentRegistry;
  try {
    switch (kind) {
      case 0: return bytes_result(encode_ids(R::engine_ids()));
      case 1: return bytes_result(encode_ids(R::state_ids()));
      case 2: return bytes_result(encode_ids(R::proposer_ids()));
      default: return error_result("registry: unknown kind (0 engines, 1 states, 2 proposers)");
    }
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_genesis_build(uint64_t chain_id, uint64_t timestamp,
                                                    uint16_t assurance_level, uint8_t state_model,
                                                    const uint8_t* account_addrs,
                                                    const uint64_t* account_balances,
                                                    size_t account_count,
                                                    const uint8_t* validator_pks,
                                                    const uint64_t* validator_powers,
                                                    size_t validator_count) {
  try {
    aviary::GenesisSpec g;
    g.chain_id = chain_id;
    g.timestamp = timestamp;
    if (state_model == 0) g.state_model = aviary::StateModel::kAccount;
    else if (state_model == 1) g.state_model = aviary::StateModel::kUtxo;
    else return error_result("genesis: state_model must be 0 (account) or 1 (utxo)");
    for (size_t i = 0; i < account_count; ++i) {
      aviary::crypto::Hash a = to_hash32(account_addrs + 32 * i);
      if (state_model == 0) g.accounts.push_back({a, account_balances[i]});
      else g.utxos.push_back({a, account_balances[i]});
    }
    for (size_t i = 0; i < validator_count; ++i) {
      g.validators.push_back(aviary::consensus::Validator{
          aviary::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)),
          validator_powers[i]});
    }
    switch (assurance_level) {
      case 1: aviary::AssuranceProfile::standard().apply_to(g); break;
      case 2: aviary::AssuranceProfile::hardened().apply_to(g); break;
      case 3: aviary::AssuranceProfile::fortress().apply_to(g); break;
      default: return error_result("genesis: assurance_level must be 1, 2 or 3");
    }
    return bytes_result(g.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_genesis_commit_blueprint(const uint8_t* genesis,
                                                               size_t genesis_len,
                                                               const uint8_t* blueprint,
                                                               size_t blueprint_len) {
  try {
    aviary::GenesisSpec g = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    aviary::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len)).commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_genesis_hash(const uint8_t* genesis, size_t genesis_len) {
  try {
    aviary::crypto::Hash h = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len)).hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryChain aviary_chain_assemble(const uint8_t* genesis, size_t genesis_len, uint32_t me,
                                              const uint8_t* bls_pub, size_t pub_len,
                                              const uint8_t* bls_sec, size_t sec_len) {
  try {
    aviary::GenesisSpec g = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    aviary::crypto::KeyPair key{aviary::crypto::Bytes(bls_pub, bls_pub + pub_len),
                                      aviary::crypto::Bytes(bls_sec, bls_sec + sec_len)};
    auto chain = aviary::node::ChainAssembler::assemble(g, me, key);
    return new ChainBox{std::move(chain), me, std::move(key)};
  } catch (...) {
    return nullptr;  // fail-closed: unknown id / forbidden mix / bad genesis
  }
}

uint64_t aviary_chain_height(AviaryChain c) {
  return c ? as_chain(c)->engine->height() : 0;
}

AviaryBufferResult aviary_chain_blueprint(AviaryChain c) {
  if (!c) return error_result("null chain");
  return bytes_result(as_chain(c)->blueprint.encode());
}

AviaryBufferResult aviary_chain_propose(AviaryChain c, uint64_t timestamp) {
  if (!c) return error_result("null chain");
  try {
    return bytes_result(as_chain(c)->engine->propose({}, timestamp).encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

AviaryBufferResult aviary_chain_seal(AviaryChain c, const uint8_t* block,
                                                 size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    aviary::Block b = aviary::Block::decode_all(cbv(block, block_len));
    auto* e = as_chain(c)->engine.get();
    if (auto* pos = dynamic_cast<aviary::consensus::PosEngine*>(e)) return bytes_result(pos->seal(b));
    if (auto* vrf = dynamic_cast<aviary::consensus::VrfPosEngine*>(e)) return bytes_result(vrf->seal(b));
    if (auto* fe = dynamic_cast<aviary::node::ForeignEngine*>(e)) return bytes_result(fe->justify(b));
    const std::uint8_t empty = 0;
    return bytes_result(&empty, 0);  // this engine does not seal
  } catch (const std::exception& ex) {
    return error_result(ex.what());
  }
}

AviaryBufferResult aviary_chain_mine(AviaryChain c, const uint8_t* block,
                                                 size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    auto* pow = dynamic_cast<aviary::consensus::PowEngine*>(as_chain(c)->engine.get());
    if (!pow) return error_result("chain: engine is not proof-of-work");
    aviary::Block b = aviary::Block::decode_all(cbv(block, block_len));
    if (!pow->mine(b)) return error_result("chain: no solving nonce found within the bound");
    return bytes_result(b.encode());
  } catch (const std::exception& ex) {
    return error_result(ex.what());
  }
}

int aviary_chain_submit(AviaryChain c, const uint8_t* block, size_t block_len,
                              const uint8_t* justification, size_t justification_len) {
  if (!c) return 0;
  try {
    aviary::Block b = aviary::Block::decode_all(cbv(block, block_len));
    return as_chain(c)->engine->submit(b, cbv(justification, justification_len)).ok ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

void aviary_chain_free(AviaryChain c) { delete as_box(c); }

// ── Subchains ─────────────────────────────────────────────────────────────────

AviaryBufferResult aviary_genesis_header(const uint8_t* genesis, size_t genesis_len) {
  try {
    aviary::GenesisSpec g = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    return bytes_result(aviary::ledger_from_genesis(g)->tip().encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_child_registration_encode(
    uint64_t chain_id, const uint8_t* genesis_header, size_t header_len, const uint8_t* validator_pks,
    const uint64_t* validator_powers, size_t validator_count, uint8_t child_max_depth) {
  try {
    if (validator_count > 0 && (!validator_pks || !validator_powers)) return error_result("registration: null validators");
    aviary::ChildRegistration c;
    c.chain_id = chain_id;
    c.genesis_header = aviary::BlockHeader::decode_all(cbv(genesis_header, header_len));
    for (size_t i = 0; i < validator_count; ++i)
      c.validators.push_back(aviary::consensus::Validator{
          aviary::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)), validator_powers[i]});
    c.child_max_depth = child_max_depth;
    return bytes_result(c.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_subchain_policy_encode(
    uint16_t max_children, uint8_t max_depth, uint16_t max_anchors_per_block, uint16_t max_headers_per_anchor,
    const uint8_t* registrations, size_t registrations_len) {
  try {
    aviary::SubchainPolicy p;
    p.max_children = max_children; p.max_depth = max_depth;
    p.max_anchors_per_block = max_anchors_per_block; p.max_headers_per_anchor = max_headers_per_anchor;
    for (const auto& b : decode_blob_list(cbv(registrations, registrations_len))) {
      aviary::codec::CodecReader r(cbv(b.data(), b.size()));
      p.children.push_back(aviary::ChildRegistration::decode(r));
      r.assert_fully_consumed();
    }
    return bytes_result(p.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_genesis_commit_subchain_policy(const uint8_t* genesis, size_t genesis_len,
                                                                     const uint8_t* policy, size_t policy_len) {
  try {
    aviary::GenesisSpec g = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    aviary::SubchainPolicy p = aviary::SubchainPolicy::decode_all(cbv(policy, policy_len));
    if (std::string e = p.validate(g.chain_id); !e.empty()) return error_result(e.c_str());
    p.commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_genesis_commit_parent_binding(const uint8_t* genesis, size_t genesis_len,
                                                                    uint64_t parent_chain_id,
                                                                    const uint8_t* parent_genesis_hash) {
  try {
    if (!parent_genesis_hash) return error_result("binding: null parent genesis hash");
    aviary::GenesisSpec g = aviary::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    aviary::ParentBinding{parent_chain_id, to_hash32(parent_genesis_hash)}.commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_block_header(const uint8_t* block, size_t block_len) {
  try { return bytes_result(aviary::Block::decode_all(cbv(block, block_len)).header.encode()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_header_subchain_root(const uint8_t* header, size_t header_len) {
  try {
    aviary::BlockHeader h = aviary::BlockHeader::decode_all(cbv(header, header_len));
    return bytes_result(h.subchain_root.data(), h.subchain_root.size());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_certified_header_encode(const uint8_t* header, size_t header_len,
                                                              const uint8_t* certificate, size_t certificate_len) {
  try {
    aviary::CertifiedHeader c{aviary::BlockHeader::decode_all(cbv(header, header_len)),
                                    aviary::crypto::Bytes(certificate, certificate + certificate_len)};
    return bytes_result(c.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_anchor_encode(uint64_t child_chain_id, const uint8_t* certified_headers,
                                                    size_t certified_headers_len) {
  try {
    aviary::Anchor a;
    a.child_chain_id = child_chain_id;
    for (const auto& b : decode_blob_list(cbv(certified_headers, certified_headers_len)))
      a.headers.push_back(aviary::CertifiedHeader::decode_all(cbv(b.data(), b.size())));
    return bytes_result(a.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_chain_propose_anchored(AviaryChain c, uint64_t timestamp,
                                                             const uint8_t* anchors, size_t anchors_len) {
  if (!c) return error_result("null chain");
  try {
    return bytes_result(as_chain(c)->engine->propose_anchored({}, timestamp, decode_anchors(cbv(anchors, anchors_len))).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_chain_certify(AviaryChain c, const uint8_t* block, size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    auto* bft = dynamic_cast<aviary::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return error_result("chain: engine is not BFT (only BFT blocks carry commit certificates)");
    aviary::Block b = aviary::Block::decode_all(cbv(block, block_len));
    ChainBox* box = as_box(c);
    // Our index follows rotations (and we may have left the set).
    auto idx = bft->validators().index_of(box->key.public_key);
    if (!idx) return error_result("chain: this node is not in the current validator set");
    aviary::consensus::Vote vote = bft->precommit(b, *idx, box->key);
    auto cert = bft->try_commit(b, {vote});
    if (!cert) return error_result("chain: this validator alone does not reach quorum; gather the other precommits");
    return bytes_result(cert->encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

// ── Light client ──────────────────────────────────────────────────────────────

namespace {
aviary::client::LightClient* as_lc(AviaryLightClient lc) {
  return static_cast<aviary::client::LightClient*>(lc);
}
}  // namespace

AviaryLightClient aviary_light_client_new(uint64_t chain_id, const uint8_t* validators,
                                                      size_t validators_len, const uint8_t* trusted_header,
                                                      size_t header_len) {
  try {
    return new aviary::client::LightClient(
        chain_id, aviary::consensus::ValidatorSet::decode_all(cbv(validators, validators_len)),
        aviary::BlockHeader::decode_all(cbv(trusted_header, header_len)));
  } catch (...) { return nullptr; }
}

int aviary_light_client_accept(AviaryLightClient lc, const uint8_t* header, size_t header_len,
                                     const uint8_t* certificate, size_t certificate_len) {
  if (!lc) return 0;
  try {
    return as_lc(lc)->accept(aviary::BlockHeader::decode_all(cbv(header, header_len)),
                             aviary::consensus::CommitCertificate::decode_all(cbv(certificate, certificate_len))).ok ? 1 : 0;
  } catch (...) { return 0; }
}

int aviary_light_client_accept_skipping(AviaryLightClient lc, const uint8_t* header, size_t header_len,
                                              const uint8_t* certificate, size_t certificate_len,
                                              const uint8_t* validators, size_t validators_len) {
  if (!lc) return 0;
  try {
    aviary::client::LightClient::Result r = as_lc(lc)->accept_skipping(
        aviary::BlockHeader::decode_all(cbv(header, header_len)),
        aviary::consensus::CommitCertificate::decode_all(cbv(certificate, certificate_len)),
        aviary::consensus::ValidatorSet::decode_all(cbv(validators, validators_len)));
    if (r.ok) return 1;
    return r.insufficient_overlap ? 2 : 0;
  } catch (...) { return 0; }
}

uint64_t aviary_light_client_height(AviaryLightClient lc) { return lc ? as_lc(lc)->height() : 0; }

AviaryBufferResult aviary_light_client_tip(AviaryLightClient lc) {
  if (!lc) return error_result("null light client");
  return bytes_result(as_lc(lc)->tip().encode());
}

AviaryBufferResult aviary_light_client_validators(AviaryLightClient lc) {
  if (!lc) return error_result("null light client");
  return bytes_result(as_lc(lc)->validators().encode());
}

void aviary_light_client_set_trust_level(AviaryLightClient lc, uint32_t numerator, uint32_t denominator) {
  if (lc) as_lc(lc)->set_trust_level(numerator, denominator);
}
void aviary_light_client_set_max_skip(AviaryLightClient lc, uint64_t blocks) {
  if (lc) as_lc(lc)->set_max_skip(blocks);
}
void aviary_light_client_free(AviaryLightClient lc) { delete as_lc(lc); }

int aviary_chain_schedule_validators(AviaryChain c, const uint8_t* validator_pks,
                                           const uint64_t* validator_powers, size_t validator_count) {
  if (!c || !validator_pks || !validator_powers || validator_count == 0) return 0;
  try {
    auto* bft = dynamic_cast<aviary::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return 0;
    std::vector<aviary::consensus::Validator> vs;
    for (size_t i = 0; i < validator_count; ++i)
      vs.push_back(aviary::consensus::Validator{
          aviary::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)), validator_powers[i]});
    bft->schedule_validator_set(aviary::consensus::ValidatorSet(std::move(vs)));  // throws if malformed
    return 1;
  } catch (...) { return 0; }
}

AviaryBufferResult aviary_chain_validators(AviaryChain c) {
  if (!c) return error_result("null chain");
  try {
    auto* bft = dynamic_cast<aviary::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return error_result("chain: engine is not BFT");
    return bytes_result(bft->validators().encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_chain_child_tip(AviaryChain c, uint64_t child_chain_id) {
  if (!c) return error_result("null chain");
  const aviary::SubchainRegistry* reg = as_chain(c)->engine->ledger().subchains();
  if (!reg) return error_result("chain: not a parent (no subchain policy)");
  const aviary::BlockHeader* tip = reg->tip_of(child_chain_id);
  if (!tip) return error_result("chain: child not registered");
  return bytes_result(tip->encode());
}

AviaryBufferResult aviary_chain_subchain_hop(AviaryChain c, uint64_t child_chain_id) {
  if (!c) return error_result("null chain");
  try {
    const aviary::SubchainRegistry* reg = as_chain(c)->engine->ledger().subchains();
    if (!reg) return error_result("chain: not a parent (no subchain policy)");
    return bytes_result(reg->hop(child_chain_id).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_chain_prove_account(AviaryChain c, const uint8_t* address) {
  if (!c) return error_result("null chain");
  try {
    auto* acc = dynamic_cast<const aviary::AccountStateMachine*>(&as_chain(c)->engine->ledger().state());
    if (!acc) return error_result("chain: not an account-model chain");
    return bytes_result(acc->prove_account(to_hash32(address)).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_subchain_proof_encode(const uint8_t* hops, size_t hops_len,
                                                            const uint8_t* state_proof, size_t state_proof_len) {
  try {
    aviary::SubchainProof p;
    for (const auto& b : decode_blob_list(cbv(hops, hops_len)))
      p.hops.push_back(aviary::SubchainProof::Hop::decode_all(cbv(b.data(), b.size())));
    p.state_proof = aviary::StateProof::decode_all(cbv(state_proof, state_proof_len));
    return bytes_result(p.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

int aviary_subchain_proof_verify(const uint8_t* parent_subchain_root, const uint8_t* proof, size_t proof_len) {
  if (!parent_subchain_root || !proof) return 0;
  try {
    return aviary::verify_subchain_proof(to_hash32(parent_subchain_root),
                                               aviary::SubchainProof::decode_all(cbv(proof, proof_len))) ? 1 : 0;
  } catch (...) { return 0; }
}

// ── Chain tree ────────────────────────────────────────────────────────────────

namespace {
// The tree as seen from the C ABI: the hooks are C callbacks with a ctx.
class CallbackTree : public aviary::node::ChainTree {
 public:
  static std::unique_ptr<CallbackTree> build(const aviary::node::ChainTreeSpec& spec) {
    auto base = aviary::node::ChainTree::build(spec);
    auto t = std::unique_ptr<CallbackTree>(new CallbackTree());
    static_cast<aviary::node::ChainTree&>(*t) = std::move(*base);
    return t;
  }
  AviaryTxSourceFn tx_fn = nullptr; void* tx_ctx = nullptr;
  AviaryFinalizedFn fin_fn = nullptr; void* fin_ctx = nullptr;

 protected:
  std::vector<aviary::Transaction> select_txs(const Built& n, std::uint64_t ts) override {
    if (!tx_fn) return {};
    std::vector<std::uint8_t> buf(AVIARY_MAX_TX_LIST);
    size_t len = 0;
    if (tx_fn(tx_ctx, n.chain_id, ts, buf.data(), buf.size(), &len) != 1) return {};
    if (len > buf.size()) throw std::runtime_error("tx source overflowed its buffer");
    std::vector<aviary::Transaction> out;
    for (const auto& b : decode_blob_list(cbv(buf.data(), len)))
      out.push_back(aviary::Transaction::decode_all(cbv(b.data(), b.size())));
    return out;
  }
  void on_finalized(const Built& n, const aviary::Block& b) override {
    if (!fin_fn) return;
    aviary::crypto::Bytes e = b.encode();
    fin_fn(fin_ctx, n.chain_id, e.data(), e.size());
  }
 private:
  CallbackTree() = default;
};
CallbackTree* as_tree(AviaryChainTree t) { return static_cast<CallbackTree*>(t); }
}  // namespace

AviaryBufferResult aviary_chain_tree_node_encode(
    uint64_t chain_id, const uint8_t* blueprint, size_t blueprint_len, uint16_t assurance_level,
    const uint8_t* account_addrs, const uint64_t* account_balances, size_t account_count,
    const uint8_t* validator_pks, const uint8_t* validator_secrets, const uint64_t* validator_powers,
    size_t validator_count, uint16_t max_anchors_per_block, uint16_t max_headers_per_anchor,
    const uint8_t* children, size_t children_len) {
  try {
    aviary::node::ChainTreeSpec::Node n;
    n.chain_id = chain_id;
    n.blueprint = aviary::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len));
    n.assurance_level = assurance_level;
    if (account_count > 0 && (!account_addrs || !account_balances)) return error_result("tree node: null accounts");
    for (size_t i = 0; i < account_count; ++i)
      n.accounts.push_back(aviary::GenesisAccount{to_hash32(account_addrs + 32 * i),
                                                        static_cast<aviary::codec::u128>(account_balances[i])});
    if (validator_count > 0 && (!validator_pks || !validator_powers)) return error_result("tree node: null validators");
    for (size_t i = 0; i < validator_count; ++i) {
      aviary::node::ChainTreeSpec::Validator v;
      v.bls_public = aviary::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1));
      if (validator_secrets) {
        aviary::crypto::Bytes s(validator_secrets + 32 * i, validator_secrets + 32 * (i + 1));
        bool zero = true; for (auto b : s) zero &= (b == 0);
        if (!zero) v.bls_secret = std::move(s);
      }
      v.power = validator_powers[i];
      n.validators.push_back(std::move(v));
    }
    n.max_anchors_per_block = max_anchors_per_block;
    n.max_headers_per_anchor = max_headers_per_anchor;
    if (children_len > 0) {
      for (const auto& b : decode_blob_list(cbv(children, children_len))) {
        aviary::codec::CodecReader r(cbv(b.data(), b.size()));
        n.children.push_back(aviary::node::ChainTreeSpec::Node::decode(r));
        r.assert_fully_consumed();
      }
    }
    return bytes_result(n.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_chain_tree_spec_error(const uint8_t* spec, size_t spec_len) {
  try {
    std::string e = aviary::node::ChainTreeSpec::decode_all(cbv(spec, spec_len)).validate();
    return bytes_result(reinterpret_cast<const std::uint8_t*>(e.data()), e.size());
  } catch (const std::exception& e) {
    std::string m = std::string("malformed spec: ") + e.what();
    return bytes_result(reinterpret_cast<const std::uint8_t*>(m.data()), m.size());
  }
}

AviaryChainTree aviary_chain_tree_build(const uint8_t* spec, size_t spec_len) {
  try {
    return CallbackTree::build(aviary::node::ChainTreeSpec::decode_all(cbv(spec, spec_len))).release();
  } catch (...) { return nullptr; }
}

AviaryBufferResult aviary_chain_tree_chain_ids(AviaryChainTree t) {
  if (!t) return error_result("null tree");
  aviary::codec::CodecWriter w;
  w.write_list(as_tree(t)->chain_ids(), [](aviary::codec::CodecWriter& ww, std::uint64_t id) { ww.write_u64(id); });
  return bytes_result(w.take());
}
uint64_t aviary_chain_tree_height(AviaryChainTree t, uint64_t chain_id) {
  if (!t || !as_tree(t)->has(chain_id)) return 0;
  return as_tree(t)->height(chain_id);
}
AviaryBufferResult aviary_chain_tree_genesis(AviaryChainTree t, uint64_t chain_id) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->node(chain_id).genesis.encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_chain_tree_tip(AviaryChainTree t, uint64_t chain_id) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->node(chain_id).chain.engine->tip().encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
uint32_t aviary_chain_tree_pending_anchors(AviaryChainTree t, uint64_t chain_id) {
  if (!t || !as_tree(t)->has(chain_id)) return 0;
  return static_cast<uint32_t>(as_tree(t)->pending_anchors_for(chain_id).size());
}
AviaryBufferResult aviary_chain_tree_produce(AviaryChainTree t, uint64_t chain_id, uint64_t timestamp) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->produce(chain_id, timestamp).encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_chain_tree_prove_account(AviaryChainTree t, uint64_t chain_id, const uint8_t* address) {
  if (!t) return error_result("null tree");
  if (!address) return error_result("null address");
  try { return bytes_result(as_tree(t)->prove_account(chain_id, to_hash32(address)).encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
int aviary_chain_tree_verify(AviaryChainTree t, const uint8_t* proof, size_t proof_len) {
  if (!t || !proof) return 0;
  try { return as_tree(t)->verify(aviary::SubchainProof::decode_all(cbv(proof, proof_len))) ? 1 : 0; } catch (...) { return 0; }
}
void aviary_chain_tree_set_tx_source(AviaryChainTree t, AviaryTxSourceFn fn, void* ctx) {
  if (t) { as_tree(t)->tx_fn = fn; as_tree(t)->tx_ctx = ctx; }
}
void aviary_chain_tree_set_finalized_hook(AviaryChainTree t, AviaryFinalizedFn fn, void* ctx) {
  if (t) { as_tree(t)->fin_fn = fn; as_tree(t)->fin_ctx = ctx; }
}
void aviary_chain_tree_free(AviaryChainTree t) { delete as_tree(t); }

// ── Foreign components (C callbacks → registry) ───────────────────────────────

int aviary_registry_add_engine(const char* id, AviaryJustifyFn justify,
                                     AviaryVerifyFn verify, void* ctx, int grind_proof) {
  if (!id || !*id || !justify || !verify) return 0;
  try {
    aviary::node::register_foreign_engine(
        id,
        [justify, ctx](const aviary::Block& b) -> aviary::crypto::Bytes {
          const aviary::crypto::Bytes enc = b.encode();
          std::vector<std::uint8_t> out(AVIARY_MAX_JUSTIFICATION);
          size_t n = 0;
          if (justify(ctx, enc.data(), enc.size(), out.data(), out.size(), &n) != 1 || n > out.size())
            throw std::runtime_error("foreign justify refused");
          out.resize(n);
          return out;
        },
        [verify, ctx](const aviary::Block& b, aviary::crypto::ByteView j) -> bool {
          const aviary::crypto::Bytes enc = b.encode();
          return verify(ctx, enc.data(), enc.size(), j.data(), j.size()) == 1;
        },
        grind_proof != 0);
    return 1;
  } catch (...) {
    return 0;
  }
}

int aviary_registry_add_proposer(const char* id, AviaryProposerFn fn, void* ctx) {
  if (!id || !*id || !fn) return 0;
  try {
    aviary::node::register_foreign_proposer(
        id, [fn, ctx](std::uint64_t height, std::uint32_t n) { return fn(ctx, height, n); });
    return 1;
  } catch (...) {
    return 0;
  }
}

// ── Interop addresses ─────────────────────────────────────────────────────────

AviaryBufferResult aviary_eth_address(const uint8_t* secret32) {
  try {
    auto a = aviary::interop::evm::address_from_secret(aviary::codec::ByteView(secret32, 32));
    return string_result(aviary::interop::evm::to_checksum_address(a));
  } catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_btc_address(const uint8_t* secret32) {
  try {
    return string_result(aviary::interop::bitcoin::p2wpkh_address_from_secret(
        aviary::codec::ByteView(secret32, 32)));
  } catch (const std::exception& e) { return error_result(e.what()); }
}
AviaryBufferResult aviary_sol_address(const uint8_t* ed_secret64) {
  try {
    aviary::interop::solana::Pubkey pk{};
    std::memcpy(pk.data(), ed_secret64 + 32, 32);  // libsodium sk = seed‖pub
    return string_result(aviary::interop::solana::to_address(pk));
  } catch (const std::exception& e) { return error_result(e.what()); }
}

// ── EVM smart-contract interaction ────────────────────────────────────────────

AviaryBufferResult aviary_eth_erc20_transfer(const uint8_t* to20, uint64_t amount) {
  try {
    namespace abi = aviary::interop::abi;
    aviary::interop::evm::Address to{};
    std::memcpy(to.data(), to20, 20);
    auto data = abi::encode_call("transfer(address,uint256)",
                                 {abi::encode_address(to),
                                  abi::encode_uint(static_cast<aviary::codec::u128>(amount))});
    return bytes_result(data);
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_eth_selector(const char* signature) {
  try {
    return bytes_result(aviary::interop::abi::selector(signature));
  } catch (const std::exception& e) { return error_result(e.what()); }
}

AviaryBufferResult aviary_eth_sign_legacy(
    uint64_t nonce, uint64_t gas_price, uint64_t gas_limit, const uint8_t* to20,
    int has_to, uint64_t value, const uint8_t* data, size_t data_len,
    uint64_t chain_id, const uint8_t* secret32) {
  try {
    aviary::interop::evm::LegacyTx tx;
    tx.nonce = nonce;
    tx.gas_price = static_cast<aviary::codec::u128>(gas_price);
    tx.gas_limit = gas_limit;
    if (has_to) {
      aviary::interop::evm::Address to{};
      std::memcpy(to.data(), to20, 20);
      tx.to = to;
    }
    tx.value = static_cast<aviary::codec::u128>(value);
    if (data != nullptr && data_len > 0) tx.data.assign(data, data + data_len);
    tx.chain_id = chain_id;
    auto st = aviary::interop::evm::sign(tx, aviary::codec::ByteView(secret32, 32));
    return bytes_result(st.raw);
  } catch (const std::exception& e) { return error_result(e.what()); }
}

void aviary_buffer_free(AviaryBuffer* buf) {
  if (buf != nullptr && buf->data != nullptr) {
    std::free(buf->data);
    buf->data = nullptr;
    buf->len = 0;
  }
}

void aviary_str_free(char* str) { std::free(str); }

}  // extern "C"
