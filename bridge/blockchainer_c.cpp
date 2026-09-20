#include "blockchainer_c.h"

#include <cstdlib>
#include <cstring>
#include <string>

#include <vector>

#include "blockchainer/consensus.hpp"
#include "blockchainer/crypto.hpp"
#include "blockchainer/interop/bitcoin.hpp"
#include "blockchainer/interop/abi.hpp"
#include "blockchainer/interop/evm.hpp"
#include "blockchainer/interop/solana.hpp"
#include "blockchainer/keyring.hpp"
#include "blockchainer/assurance.hpp"
#include "blockchainer/chain_assembler.hpp"
#include "blockchainer/chain_blueprint.hpp"
#include "blockchainer/foreign_engine.hpp"
#include "blockchainer/account_state.hpp"
#include "blockchainer/bft_engine.hpp"
#include "blockchainer/subchain.hpp"
#include "blockchainer/light_client.hpp"
#include "blockchainer/chain_tree.hpp"
#include "blockchainer/genesis.hpp"
#include "blockchainer/pos_engine.hpp"
#include "blockchainer/pow_engine.hpp"
#include "blockchainer/vrf_pos.hpp"
#include "blockchainer/credential_gate.hpp"
#include "blockchainer/node/demo_node.hpp"
#include "blockchainer/proofs.hpp"
#include "blockchainer/sealed_mempool.hpp"
#include "blockchainer/threshold.hpp"
#include "blockchainer/codec.hpp"
#include "blockchainer/types.hpp"
#include "blockchainer/version.hpp"

namespace {

// Duplicate a std::string into a malloc'd C string (caller frees via
// blockchainer_str_free). Returns nullptr on allocation failure.
char* dup_cstr(const std::string& s) {
  char* p = static_cast<char*>(std::malloc(s.size() + 1));
  if (p == nullptr) return nullptr;
  std::memcpy(p, s.c_str(), s.size() + 1);
  return p;
}

BlockchainerBufferResult error_result(const std::string& msg) {
  return BlockchainerBufferResult{{nullptr, 0}, dup_cstr(msg)};
}

// Copy raw bytes into a malloc'd buffer result.
BlockchainerBufferResult bytes_result(const std::uint8_t* data, std::size_t len) {
  auto* out = static_cast<std::uint8_t*>(std::malloc(len == 0 ? 1 : len));
  if (out == nullptr) return error_result("out of memory");
  std::memcpy(out, data, len);
  return BlockchainerBufferResult{{out, len}, nullptr};
}
BlockchainerBufferResult bytes_result(const std::vector<std::uint8_t>& b) {
  return bytes_result(b.data(), b.size());
}
BlockchainerBufferResult string_result(const std::string& s) {
  return bytes_result(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

blockchainer::crypto::Hash to_hash32(const std::uint8_t* p) {
  blockchainer::crypto::Hash h{};
  std::memcpy(h.data(), p, 32);
  return h;
}

blockchainer::node::DemoNode* as_node(BlockchainerNode n) {
  return static_cast<blockchainer::node::DemoNode*>(n);
}

blockchainer::custody::KeyStore* as_ks(BlockchainerKeyStore k) {
  return static_cast<blockchainer::custody::KeyStore*>(k);
}

// Pack a {public_key, secret} pair as the self-describing account blob
// [pub_len:4 BE][public][secret] — the same layout new_account_hybrid emits.
BlockchainerBufferResult account_blob(const blockchainer::crypto::Bytes& pub,
                                      const blockchainer::crypto::Bytes& sec) {
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

blockchainer::crypto::ByteView cbv(const std::uint8_t* p, std::size_t n) {
  return blockchainer::crypto::ByteView(p, n);
}

namespace cg = blockchainer::consensus;

cg::CredentialGate* as_gate(BlockchainerCredentialGate g) {
  return static_cast<cg::CredentialGate*>(g);
}

// Credential blob = [sig_len:4][sig]([attr_count:4]([attr_len:4][attr])×).
// Holder-secret (carries member_secret + hidden attributes).
std::vector<std::uint8_t> encode_credential(const cg::Credential& c) {
  blockchainer::codec::CodecWriter w;
  w.write_bytes(cbv(c.signature.data(), c.signature.size()));
  w.write_list(c.attrs,
               [](blockchainer::codec::CodecWriter& ww, const blockchainer::crypto::Bytes& a) {
                 ww.write_bytes(cbv(a.data(), a.size()));
               });
  return w.take();
}
cg::Credential decode_credential(const std::uint8_t* p, std::size_t n) {
  blockchainer::codec::CodecReader r(cbv(p, n));
  cg::Credential c;
  c.signature = r.read_bytes();
  c.attrs = r.read_list<blockchainer::crypto::Bytes>(
      [](blockchainer::codec::CodecReader& rr) { return rr.read_bytes(); });
  r.assert_fully_consumed();
  return c;
}

// ── Option B blob (de)serialisers ──────────────────────────────────────────
using CW = blockchainer::codec::CodecWriter;
using CR = blockchainer::codec::CodecReader;

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
  w.write_list(c.nym_secrets, [](CW& ww, const blockchainer::crypto::Bytes& s) {
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
  c.nym_secrets = r.read_list<blockchainer::crypto::Bytes>([](CR& rr) { return rr.read_bytes(); });
  c.secret_prover_blind = r.read_bytes();
  r.assert_fully_consumed();
  return c;
}


// The chain handle: the assembled chain plus what this validator needs to
// certify its own BFT blocks (its index + BLS key).
struct ChainBox {
  blockchainer::node::AssembledChain chain;
  std::uint32_t me;
  blockchainer::crypto::KeyPair key;
};
ChainBox* as_box(BlockchainerChain c) { return static_cast<ChainBox*>(c); }
blockchainer::node::AssembledChain* as_chain(BlockchainerChain c) { return &as_box(c)->chain; }
// [count u32]([len u32][bytes])× → the items.
std::vector<blockchainer::crypto::Bytes> decode_blob_list(blockchainer::codec::ByteView v) {
  blockchainer::codec::CodecReader r(v);
  auto items = r.read_list<blockchainer::crypto::Bytes>([](blockchainer::codec::CodecReader& rr) { return rr.read_bytes(); });
  r.assert_fully_consumed();
  return items;
}
std::vector<blockchainer::Anchor> decode_anchors(blockchainer::codec::ByteView v) {
  std::vector<blockchainer::Anchor> out;
  for (const auto& b : decode_blob_list(v)) out.push_back(blockchainer::Anchor::decode_all(cbv(b.data(), b.size())));
  return out;
}
// [count u32]([len u32][id])×
std::vector<std::uint8_t> encode_ids(const std::vector<std::string>& ids) {
  blockchainer::codec::CodecWriter w;
  w.write_list(ids, [](blockchainer::codec::CodecWriter& ww, const std::string& id) {
    ww.write_bytes(cbv(reinterpret_cast<const std::uint8_t*>(id.data()), id.size()));
  });
  return w.take();
}
}  // namespace

extern "C" {

int blockchainer_init(void) {
  try {
    blockchainer::init();
    return 0;
  } catch (...) {
    return 1;
  }
}

const char* blockchainer_version(void) {
  // Static storage so the returned pointer outlives the call.
  static const std::string v = blockchainer::version();
  return v.c_str();
}

const char* blockchainer_crypto_backend_version(void) {
  static const std::string v = blockchainer::crypto_backend_version();
  return v.c_str();
}

BlockchainerBufferResult blockchainer_hash(const uint8_t* msg, size_t len) {
  try {
    const blockchainer::crypto::Hash h = blockchainer::crypto::blake3_256(msg, len);
    auto* out = static_cast<uint8_t*>(std::malloc(h.size()));
    if (out == nullptr) return error_result("out of memory");
    std::memcpy(out, h.data(), h.size());
    return BlockchainerBufferResult{{out, h.size()}, nullptr};
  } catch (const std::exception& e) {
    return error_result(e.what());
  } catch (...) {
    return error_result("unknown error");
  }
}

namespace {
// Copy a 32-byte hash into a fresh malloc'd buffer result.
BlockchainerBufferResult hash_result(const blockchainer::crypto::Hash& h) {
  auto* out = static_cast<uint8_t*>(std::malloc(h.size()));
  if (out == nullptr) return error_result("out of memory");
  std::memcpy(out, h.data(), h.size());
  return BlockchainerBufferResult{{out, h.size()}, nullptr};
}

blockchainer::Bytes to_bytes(const uint8_t* p, size_t n) {
  return p == nullptr ? blockchainer::Bytes{} : blockchainer::Bytes(p, p + n);
}
}  // namespace

BlockchainerBufferResult blockchainer_txid(uint8_t version, uint64_t chain_id,
                                           const uint8_t* body, size_t body_len,
                                           const uint8_t* author, size_t author_len,
                                           const uint8_t* signature, size_t sig_len) {
  try {
    blockchainer::Transaction tx;
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

BlockchainerBufferResult blockchainer_header_hash(
    uint8_t version, uint64_t chain_id, uint64_t height,
    const uint8_t* parent_hash, uint64_t timestamp,
    const uint8_t* tx_root, const uint8_t* state_root,
    const uint8_t* consensus_data, size_t consensus_len) {
  try {
    blockchainer::BlockHeader h;
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
blockchainer::codec::ByteView cview(const uint8_t* p, size_t n) {
  return blockchainer::codec::ByteView(p, n);
}
blockchainer::crypto::Hash hash32(const uint8_t* p) {
  blockchainer::crypto::Hash h{};
  std::memcpy(h.data(), p, 32);
  return h;
}
}  // namespace

int blockchainer_verify_state_proof(const uint8_t* state_root, const uint8_t* proof,
                                    size_t proof_len) {
  try {
    auto p = blockchainer::StateProof::decode_all(cview(proof, proof_len));
    return blockchainer::verify_state_proof(hash32(state_root), p) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int blockchainer_verify_tx_inclusion(const uint8_t* tx_root, const uint8_t* proof,
                                     size_t proof_len) {
  try {
    auto p = blockchainer::TxInclusionProof::decode_all(cview(proof, proof_len));
    return blockchainer::verify_tx_inclusion(hash32(tx_root), p) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

int blockchainer_verify_commit(uint64_t chain_id, const uint8_t* validator_set,
                               size_t vset_len, const uint8_t* cert, size_t cert_len) {
  try {
    auto vset = blockchainer::consensus::ValidatorSet::decode_all(cview(validator_set, vset_len));
    auto c = blockchainer::consensus::CommitCertificate::decode_all(cview(cert, cert_len));
    return blockchainer::consensus::verify_commit(chain_id, vset, c).ok ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

// ── Demo node ─────────────────────────────────────────────────────────────────

BlockchainerNode blockchainer_node_new(uint64_t chain_id) {
  try {
    return new blockchainer::node::DemoNode(chain_id);
  } catch (...) {
    return nullptr;
  }
}

void blockchainer_node_free(BlockchainerNode node) { delete as_node(node); }

BlockchainerBufferResult blockchainer_node_new_account(BlockchainerNode node) {
  try {
    auto kp = as_node(node)->new_account();  // secret 64, public 32
    return bytes_result(kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_account_address(const uint8_t* public_key, size_t pk_len) {
  try {
    auto h = blockchainer::crypto::address_of(blockchainer::codec::ByteView(public_key, pk_len));
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int blockchainer_node_credit(BlockchainerNode node, const uint8_t* address, uint64_t amount) {
  try {
    as_node(node)->credit(to_hash32(address), static_cast<blockchainer::codec::u128>(amount));
    return BLOCKCHAINER_OK;
  } catch (const blockchainer::node::GenesisSealedError&) {
    return BLOCKCHAINER_ERR_GENESIS_SEALED;
  } catch (...) {
    return BLOCKCHAINER_ERR;
  }
}

BlockchainerBufferResult blockchainer_node_submit_transfer(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce) {
  try {
    auto txid = as_node(node)->submit_transfer(
        blockchainer::codec::ByteView(secret, sk_len), to_hash32(to_address),
        static_cast<blockchainer::codec::u128>(amount), nonce);
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_node_submit_transfer_fee(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce, uint64_t fee) {
  try {
    auto txid = as_node(node)->submit_transfer_fee(
        blockchainer::codec::ByteView(secret, sk_len), to_hash32(to_address),
        static_cast<blockchainer::codec::u128>(amount), nonce,
        static_cast<blockchainer::codec::u128>(fee));
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_node_new_account_hybrid(BlockchainerNode node) {
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

BlockchainerBufferResult blockchainer_node_submit_transfer_hybrid(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* public_key, size_t pk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce) {
  try {
    auto txid = as_node(node)->submit_transfer_hybrid(
        blockchainer::codec::ByteView(secret, sk_len),
        blockchainer::codec::ByteView(public_key, pk_len), to_hash32(to_address),
        static_cast<blockchainer::codec::u128>(amount), nonce);
    return bytes_result(txid.data(), txid.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int blockchainer_node_produce_block(BlockchainerNode node, uint64_t timestamp, uint64_t* out_height) {
  try {
    uint64_t h = as_node(node)->produce_block(timestamp);
    if (out_height) *out_height = h;
    return 0;
  } catch (...) {
    return 1;
  }
}

uint64_t blockchainer_node_height(BlockchainerNode node) {
  try { return as_node(node)->height(); } catch (...) { return 0; }
}
uint64_t blockchainer_node_balance(BlockchainerNode node, const uint8_t* address) {
  try { return static_cast<uint64_t>(as_node(node)->balance(to_hash32(address))); }
  catch (...) { return 0; }
}
uint64_t blockchainer_node_nonce(BlockchainerNode node, const uint8_t* address) {
  try { return as_node(node)->nonce(to_hash32(address)); } catch (...) { return 0; }
}
uint64_t blockchainer_node_mempool_size(BlockchainerNode node) {
  try { return as_node(node)->mempool_size(); } catch (...) { return 0; }
}

BlockchainerBufferResult blockchainer_node_state_root(BlockchainerNode node) {
  try {
    auto h = as_node(node)->state_root();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_node_drain_events(BlockchainerNode node) {
  try { return string_result(as_node(node)->drain_events_json()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_node_metrics(BlockchainerNode node) {
  try { return string_result(as_node(node)->metrics_text()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_node_prove_account(BlockchainerNode node, const uint8_t* address) {
  try { return bytes_result(as_node(node)->prove_account(to_hash32(address)).encode()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}

// ── Custodial key store ───────────────────────────────────────────────────────

BlockchainerKeyStore blockchainer_keystore_new(const char* passphrase) {
  try {
    return new blockchainer::custody::KeyStore(
        blockchainer::custody::KeyStore::create_with_passphrase(passphrase ? passphrase : ""));
  } catch (...) {
    return nullptr;
  }
}

int blockchainer_keystore_add_account(BlockchainerKeyStore ks, const uint8_t* public_key,
                                      size_t pk_len, const uint8_t* secret, size_t sk_len,
                                      uint8_t* out_address) {
  try {
    blockchainer::crypto::Hash addr = as_ks(ks)->add_account(
        blockchainer::crypto::ByteView(public_key, pk_len),
        blockchainer::crypto::ByteView(secret, sk_len));
    std::memcpy(out_address, addr.data(), 32);
    return 0;
  } catch (...) {
    return 1;
  }
}

BlockchainerBufferResult blockchainer_keystore_reveal(BlockchainerKeyStore ks,
                                                      const uint8_t* address) {
  try {
    return bytes_result(as_ks(ks)->reveal_secret(to_hash32(address)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_keystore_serialise(BlockchainerKeyStore ks) {
  try {
    return bytes_result(as_ks(ks)->serialise());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerKeyStore blockchainer_keystore_load(const uint8_t* blob, size_t len) {
  try {
    return new blockchainer::custody::KeyStore(
        blockchainer::custody::KeyStore::load(blockchainer::crypto::ByteView(blob, len)));
  } catch (...) {
    return nullptr;
  }
}

int blockchainer_keystore_unlock(BlockchainerKeyStore ks, const char* passphrase) {
  try {
    as_ks(ks)->unlock_with_passphrase(passphrase ? passphrase : "");
    return 0;
  } catch (...) {
    return 1;
  }
}

int blockchainer_keystore_unlocked(BlockchainerKeyStore ks) {
  return as_ks(ks)->unlocked() ? 1 : 0;
}

void blockchainer_keystore_free(BlockchainerKeyStore ks) { delete as_ks(ks); }

// ── Portable account export (tiered encrypted backup) ─────────────────────────

BlockchainerBufferResult blockchainer_hybrid_kem_keygen(void) {
  try {
    auto kp = blockchainer::crypto::hybrid_kem_keygen();
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_seal_passphrase(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const char* passphrase, uint64_t ops, size_t mem) {
  try {
    blockchainer::custody::PortableAccount acct{
        blockchainer::crypto::Bytes(public_key, public_key + pk_len),
        blockchainer::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_passphrase(passphrase ? passphrase : "", ops, mem));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_open_passphrase(
    const uint8_t* envelope, size_t env_len, const char* passphrase) {
  try {
    auto a = blockchainer::custody::PortableAccount::open_passphrase(cbv(envelope, env_len),
                                                                    passphrase ? passphrase : "");
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_seal_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len) {
  try {
    blockchainer::custody::PortableAccount acct{
        blockchainer::crypto::Bytes(public_key, public_key + pk_len),
        blockchainer::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_to(cbv(recipient_kem_public, rk_len)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_open_to(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len) {
  try {
    auto a = blockchainer::custody::PortableAccount::open_to(cbv(envelope, env_len),
                                                            cbv(recipient_kem_secret, rk_len));
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_seal_signed_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len,
    const uint8_t* sender_sig_secret, size_t ss_len) {
  try {
    blockchainer::custody::PortableAccount acct{
        blockchainer::crypto::Bytes(public_key, public_key + pk_len),
        blockchainer::crypto::Bytes(secret, secret + sk_len)};
    return bytes_result(acct.seal_signed_to(cbv(recipient_kem_public, rk_len),
                                            cbv(sender_sig_secret, ss_len)));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_portable_open_signed_from(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len,
    const uint8_t* sender_sig_public, size_t sp_len) {
  try {
    auto a = blockchainer::custody::PortableAccount::open_signed_from(
        cbv(envelope, env_len), cbv(recipient_kem_secret, rk_len), cbv(sender_sig_public, sp_len));
    return account_blob(a.public_key, a.secret);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

// ── Threshold accounts (FROST t-of-n) ─────────────────────────────────────────

namespace {
namespace th = blockchainer::custody;

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

th::SigningCeremony* as_ceremony(BlockchainerCeremony c) {
  return static_cast<th::SigningCeremony*>(c);
}
}  // namespace

BlockchainerBufferResult blockchainer_threshold_create(uint16_t n, uint16_t t) {
  try {
    std::vector<th::ShareCard> cards = th::ThresholdAccount::create(n, t);
    std::vector<std::uint8_t> out;
    put_u16(out, static_cast<std::uint16_t>(cards.size()));
    for (const auto& card : cards) {
      const blockchainer::crypto::Bytes enc = card.encode();
      put_u32(out, static_cast<std::uint32_t>(enc.size()));
      out.insert(out.end(), enc.begin(), enc.end());
    }
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_threshold_share_info(const uint8_t* card, size_t card_len) {
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

BlockchainerBufferResult blockchainer_threshold_share_seal(const uint8_t* card, size_t card_len,
                                                           const char* passphrase, uint64_t ops,
                                                           size_t mem) {
  try {
    th::ShareCard c = th::ShareCard::decode(cbv(card, card_len));
    return bytes_result(c.seal(passphrase ? passphrase : "", ops, mem));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_threshold_share_unseal(const uint8_t* envelope, size_t env_len,
                                                             const char* passphrase) {
  try {
    th::ShareCard c = th::ShareCard::unseal(cbv(envelope, env_len), passphrase ? passphrase : "");
    return bytes_result(c.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_threshold_commit(const uint8_t* card, size_t card_len) {
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

BlockchainerCeremony blockchainer_threshold_ceremony_new(const uint8_t* group_pk32,
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
    return new th::SigningCeremony(blockchainer::crypto::Bytes(group_pk32, group_pk32 + 32),
                                   blockchainer::crypto::Bytes(message, message + msg_len),
                                   std::move(roster));
  } catch (...) {
    return nullptr;
  }
}

void blockchainer_threshold_ceremony_free(BlockchainerCeremony c) { delete as_ceremony(c); }

BlockchainerBufferResult blockchainer_threshold_sign(BlockchainerCeremony c, const uint8_t* card,
                                                     size_t card_len, const uint8_t* nonces64) {
  try {
    th::ShareCard share = th::ShareCard::decode(cbv(card, card_len));
    th::SigningCeremony::Nonces nonces{blockchainer::crypto::Bytes(nonces64, nonces64 + 32),
                                       blockchainer::crypto::Bytes(nonces64 + 32, nonces64 + 64)};
    return bytes_result(as_ceremony(c)->sign(share, nonces));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

int blockchainer_threshold_verify_share(BlockchainerCeremony c, uint16_t id, const uint8_t* public32,
                                        const uint8_t* sig_share32) {
  try {
    return as_ceremony(c)->verify_share(id, blockchainer::crypto::Bytes(public32, public32 + 32),
                                        blockchainer::crypto::Bytes(sig_share32, sig_share32 + 32))
               ? 1
               : 0;
  } catch (...) {
    return 0;
  }
}

BlockchainerBufferResult blockchainer_threshold_aggregate(BlockchainerCeremony c,
                                                          const uint16_t* ids,
                                                          const uint8_t* sig_shares, size_t count) {
  try {
    th::SigningCeremony* cer = as_ceremony(c);
    // Reorder the caller's (id → share) pairs into the ceremony's roster order,
    // which is what SigningCeremony::aggregate expects.
    std::vector<blockchainer::crypto::Bytes> ordered;
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

BlockchainerBufferResult blockchainer_assurance_describe(uint16_t level) {
  try {
    blockchainer::AssuranceProfile p = [level]() {
      switch (level) {
        case 1: return blockchainer::AssuranceProfile::standard();
        case 2: return blockchainer::AssuranceProfile::hardened();
        case 3: return blockchainer::AssuranceProfile::fortress();
        default: throw std::runtime_error("assurance: unknown profile level");
      }
    }();
    std::vector<std::uint8_t> out;
    out.push_back(p.transport() == blockchainer::AssuranceProfile::Transport::kRatcheted ? 1 : 0);
    out.push_back(p.consensus() == blockchainer::AssuranceProfile::Consensus::kVrf ? 1 : 0);
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

BlockchainerBufferResult blockchainer_tx_encode(uint8_t version, uint64_t chain_id, uint8_t sig_algo,
                                                uint64_t fee, const uint8_t* body, size_t body_len,
                                                const uint8_t* author, size_t author_len,
                                                const uint8_t* signature, size_t sig_len) {
  try {
    blockchainer::Transaction tx;
    tx.version = version;
    tx.chain_id = chain_id;
    tx.sig_algo = sig_algo;
    tx.fee = static_cast<blockchainer::codec::u128>(fee);
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
blockchainer::p2p::SealedMempool* as_pool(BlockchainerSealedMempool m) {
  return static_cast<blockchainer::p2p::SealedMempool*>(m);
}
}  // namespace

BlockchainerBufferResult blockchainer_sealed_tx_seal(const uint8_t* tx_encoded, size_t tx_len,
                                                     const uint8_t* proposer_kem_public,
                                                     size_t pk_len) {
  try {
    blockchainer::Transaction tx = blockchainer::Transaction::decode_all(cbv(tx_encoded, tx_len));
    blockchainer::p2p::SealedTx sealed =
        blockchainer::p2p::SealedTx::seal(tx, cbv(proposer_kem_public, pk_len));
    return bytes_result(sealed.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerSealedMempool blockchainer_sealed_mempool_new(size_t capacity) {
  try {
    return new blockchainer::p2p::SealedMempool(capacity);
  } catch (...) {
    return nullptr;
  }
}

int blockchainer_sealed_mempool_add(BlockchainerSealedMempool m, const uint8_t* sealed_tx,
                                    size_t len) {
  try {
    return as_pool(m)->add(blockchainer::p2p::SealedTx::decode(cbv(sealed_tx, len))) ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

size_t blockchainer_sealed_mempool_size(BlockchainerSealedMempool m) { return as_pool(m)->size(); }

BlockchainerBufferResult blockchainer_sealed_mempool_reveal(BlockchainerSealedMempool m,
                                                            const uint8_t* proposer_kem_secret,
                                                            size_t sk_len, size_t max) {
  try {
    std::vector<blockchainer::Transaction> txs =
        as_pool(m)->reveal(cbv(proposer_kem_secret, sk_len), max);
    std::vector<std::uint8_t> out;
    put_u32(out, static_cast<std::uint32_t>(txs.size()));
    for (const auto& tx : txs) {
      const blockchainer::crypto::Bytes enc = tx.encode();
      put_u32(out, static_cast<std::uint32_t>(enc.size()));
      out.insert(out.end(), enc.begin(), enc.end());
    }
    return bytes_result(out);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

void blockchainer_sealed_mempool_free(BlockchainerSealedMempool m) { delete as_pool(m); }

// ── CredentialGate (anonymous membership admission, Option A) ──────────────────

BlockchainerBufferResult blockchainer_bbs_keygen(const uint8_t* key_material, size_t km_len) {
  try {
    blockchainer::crypto::KeyPair kp = blockchainer::crypto::bbs_keygen(cbv(key_material, km_len));
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_credential_policy_encode(uint64_t chain_id,
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

BlockchainerBufferResult blockchainer_credential_policy_hash(const uint8_t* policy,
                                                             size_t policy_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    blockchainer::crypto::Hash h = p.hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_credential_issue(const uint8_t* policy, size_t policy_len,
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

BlockchainerBufferResult blockchainer_credential_present(const uint8_t* policy, size_t policy_len,
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

BlockchainerCredentialGate blockchainer_credential_gate_new(const uint8_t* policy,
                                                            size_t policy_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    return new cg::CredentialGate(std::move(p));
  } catch (...) {
    return nullptr;
  }
}

void blockchainer_credential_gate_revoke_epoch(BlockchainerCredentialGate g, uint32_t epoch) {
  if (g) as_gate(g)->revoke_issuer_epoch(epoch);
}

int blockchainer_credential_gate_admit(BlockchainerCredentialGate g, uint32_t epoch,
                                       const char* scope, const uint8_t* challenge,
                                       size_t challenge_len, const uint8_t* presentation,
                                       size_t presentation_len, uint64_t now) {
  if (g == nullptr) return 0;
  return as_gate(g)->admit(epoch, scope ? scope : "", cbv(challenge, challenge_len),
                           cbv(presentation, presentation_len), now)
             ? 1
             : 0;
}

void blockchainer_credential_gate_free(BlockchainerCredentialGate g) { delete as_gate(g); }

// ── CredentialGate Option B — pseudonymous ────────────────────────────────────

BlockchainerBufferResult blockchainer_credential_policy_encode_ex(uint64_t chain_id,
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

BlockchainerBufferResult blockchainer_membership_commit(const uint8_t* policy, size_t policy_len,
                                                        const uint8_t* member_secret,
                                                        size_t ms_len) {
  try {
    cg::CredentialPolicy p = cg::CredentialPolicy::decode_all(cbv(policy, policy_len));
    return bytes_result(encode_membership_commit(cg::membership_commit(p, cbv(member_secret, ms_len))));
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_blind_issue(const uint8_t* policy, size_t policy_len,
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

BlockchainerBufferResult blockchainer_finalize_membership(const uint8_t* membership_commit,
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

BlockchainerBufferResult blockchainer_credential_present_pseudonymous(
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

BlockchainerBufferResult blockchainer_credential_gate_admit_pseudonymous(
    BlockchainerCredentialGate g, uint32_t epoch, const char* scope, const uint8_t* challenge,
    size_t challenge_len, const uint8_t* presentation, size_t presentation_len, uint64_t now) {
  if (g == nullptr) return error_result("null gate");
  std::optional<blockchainer::crypto::Bytes> nym =
      as_gate(g)->admit_pseudonymous(epoch, scope ? scope : "", cbv(challenge, challenge_len),
                                     cbv(presentation, presentation_len), now);
  if (!nym) {
    const std::uint8_t empty = 0;
    return bytes_result(&empty, 0);  // reject → empty buffer (len 0), no error
  }
  return bytes_result(*nym);
}

void blockchainer_credential_gate_revoke_nym(BlockchainerCredentialGate g, const uint8_t* nym,
                                             size_t nym_len) {
  if (g) as_gate(g)->revoke_nym(cbv(nym, nym_len));
}

int blockchainer_credential_gate_note_admission(BlockchainerCredentialGate g, uint32_t epoch,
                                                const char* scope, const uint8_t* nym,
                                                size_t nym_len) {
  if (g == nullptr) return 0;
  return as_gate(g)->note_admission(epoch, scope ? scope : "", cbv(nym, nym_len)) ? 1 : 0;
}


// ── ChainBlueprint + ChainAssembler (composable chains) ───────────────────────

BlockchainerBufferResult blockchainer_bls_keygen(const uint8_t* ikm, size_t ikm_len) {
  try {
    blockchainer::crypto::KeyPair kp = blockchainer::crypto::bls_keygen_from_ikm(cbv(ikm, ikm_len));
    return account_blob(kp.public_key, kp.secret_key);
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_blueprint_encode(const char* consensus_id,
                                                       const char* state_model_id,
                                                       const char* proposer_id,
                                                       const uint8_t* params, size_t params_len) {
  try {
    blockchainer::ChainBlueprint b;
    b.consensus_id = consensus_id ? consensus_id : "";
    b.state_model_id = state_model_id ? state_model_id : "";
    b.proposer_id = proposer_id ? proposer_id : "";
    if (params && params_len) b.component_params.assign(params, params + params_len);
    return bytes_result(b.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_blueprint_hash(const uint8_t* blueprint, size_t blueprint_len) {
  try {
    blockchainer::crypto::Hash h = blockchainer::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len)).hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_registry_ids(uint8_t kind) {
  using R = blockchainer::node::ComponentRegistry;
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

BlockchainerBufferResult blockchainer_genesis_build(uint64_t chain_id, uint64_t timestamp,
                                                    uint16_t assurance_level, uint8_t state_model,
                                                    const uint8_t* account_addrs,
                                                    const uint64_t* account_balances,
                                                    size_t account_count,
                                                    const uint8_t* validator_pks,
                                                    const uint64_t* validator_powers,
                                                    size_t validator_count) {
  try {
    blockchainer::GenesisSpec g;
    g.chain_id = chain_id;
    g.timestamp = timestamp;
    if (state_model == 0) g.state_model = blockchainer::StateModel::kAccount;
    else if (state_model == 1) g.state_model = blockchainer::StateModel::kUtxo;
    else return error_result("genesis: state_model must be 0 (account) or 1 (utxo)");
    for (size_t i = 0; i < account_count; ++i) {
      blockchainer::crypto::Hash a = to_hash32(account_addrs + 32 * i);
      if (state_model == 0) g.accounts.push_back({a, account_balances[i]});
      else g.utxos.push_back({a, account_balances[i]});
    }
    for (size_t i = 0; i < validator_count; ++i) {
      g.validators.push_back(blockchainer::consensus::Validator{
          blockchainer::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)),
          validator_powers[i]});
    }
    switch (assurance_level) {
      case 1: blockchainer::AssuranceProfile::standard().apply_to(g); break;
      case 2: blockchainer::AssuranceProfile::hardened().apply_to(g); break;
      case 3: blockchainer::AssuranceProfile::fortress().apply_to(g); break;
      default: return error_result("genesis: assurance_level must be 1, 2 or 3");
    }
    return bytes_result(g.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_genesis_commit_blueprint(const uint8_t* genesis,
                                                               size_t genesis_len,
                                                               const uint8_t* blueprint,
                                                               size_t blueprint_len) {
  try {
    blockchainer::GenesisSpec g = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    blockchainer::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len)).commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_genesis_hash(const uint8_t* genesis, size_t genesis_len) {
  try {
    blockchainer::crypto::Hash h = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len)).hash();
    return bytes_result(h.data(), h.size());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerChain blockchainer_chain_assemble(const uint8_t* genesis, size_t genesis_len, uint32_t me,
                                              const uint8_t* bls_pub, size_t pub_len,
                                              const uint8_t* bls_sec, size_t sec_len) {
  try {
    blockchainer::GenesisSpec g = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    blockchainer::crypto::KeyPair key{blockchainer::crypto::Bytes(bls_pub, bls_pub + pub_len),
                                      blockchainer::crypto::Bytes(bls_sec, bls_sec + sec_len)};
    auto chain = blockchainer::node::ChainAssembler::assemble(g, me, key);
    return new ChainBox{std::move(chain), me, std::move(key)};
  } catch (...) {
    return nullptr;  // fail-closed: unknown id / forbidden mix / bad genesis
  }
}

uint64_t blockchainer_chain_height(BlockchainerChain c) {
  return c ? as_chain(c)->engine->height() : 0;
}

BlockchainerBufferResult blockchainer_chain_blueprint(BlockchainerChain c) {
  if (!c) return error_result("null chain");
  return bytes_result(as_chain(c)->blueprint.encode());
}

BlockchainerBufferResult blockchainer_chain_propose(BlockchainerChain c, uint64_t timestamp) {
  if (!c) return error_result("null chain");
  try {
    return bytes_result(as_chain(c)->engine->propose({}, timestamp).encode());
  } catch (const std::exception& e) {
    return error_result(e.what());
  }
}

BlockchainerBufferResult blockchainer_chain_seal(BlockchainerChain c, const uint8_t* block,
                                                 size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    blockchainer::Block b = blockchainer::Block::decode_all(cbv(block, block_len));
    auto* e = as_chain(c)->engine.get();
    if (auto* pos = dynamic_cast<blockchainer::consensus::PosEngine*>(e)) return bytes_result(pos->seal(b));
    if (auto* vrf = dynamic_cast<blockchainer::consensus::VrfPosEngine*>(e)) return bytes_result(vrf->seal(b));
    if (auto* fe = dynamic_cast<blockchainer::node::ForeignEngine*>(e)) return bytes_result(fe->justify(b));
    const std::uint8_t empty = 0;
    return bytes_result(&empty, 0);  // this engine does not seal
  } catch (const std::exception& ex) {
    return error_result(ex.what());
  }
}

BlockchainerBufferResult blockchainer_chain_mine(BlockchainerChain c, const uint8_t* block,
                                                 size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    auto* pow = dynamic_cast<blockchainer::consensus::PowEngine*>(as_chain(c)->engine.get());
    if (!pow) return error_result("chain: engine is not proof-of-work");
    blockchainer::Block b = blockchainer::Block::decode_all(cbv(block, block_len));
    if (!pow->mine(b)) return error_result("chain: no solving nonce found within the bound");
    return bytes_result(b.encode());
  } catch (const std::exception& ex) {
    return error_result(ex.what());
  }
}

int blockchainer_chain_submit(BlockchainerChain c, const uint8_t* block, size_t block_len,
                              const uint8_t* justification, size_t justification_len) {
  if (!c) return 0;
  try {
    blockchainer::Block b = blockchainer::Block::decode_all(cbv(block, block_len));
    return as_chain(c)->engine->submit(b, cbv(justification, justification_len)).ok ? 1 : 0;
  } catch (...) {
    return 0;
  }
}

void blockchainer_chain_free(BlockchainerChain c) { delete as_box(c); }

// ── Subchains ─────────────────────────────────────────────────────────────────

BlockchainerBufferResult blockchainer_genesis_header(const uint8_t* genesis, size_t genesis_len) {
  try {
    blockchainer::GenesisSpec g = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    return bytes_result(blockchainer::ledger_from_genesis(g)->tip().encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_child_registration_encode(
    uint64_t chain_id, const uint8_t* genesis_header, size_t header_len, const uint8_t* validator_pks,
    const uint64_t* validator_powers, size_t validator_count, uint8_t child_max_depth) {
  try {
    if (validator_count > 0 && (!validator_pks || !validator_powers)) return error_result("registration: null validators");
    blockchainer::ChildRegistration c;
    c.chain_id = chain_id;
    c.genesis_header = blockchainer::BlockHeader::decode_all(cbv(genesis_header, header_len));
    for (size_t i = 0; i < validator_count; ++i)
      c.validators.push_back(blockchainer::consensus::Validator{
          blockchainer::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)), validator_powers[i]});
    c.child_max_depth = child_max_depth;
    return bytes_result(c.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_subchain_policy_encode(
    uint16_t max_children, uint8_t max_depth, uint16_t max_anchors_per_block, uint16_t max_headers_per_anchor,
    const uint8_t* registrations, size_t registrations_len) {
  try {
    blockchainer::SubchainPolicy p;
    p.max_children = max_children; p.max_depth = max_depth;
    p.max_anchors_per_block = max_anchors_per_block; p.max_headers_per_anchor = max_headers_per_anchor;
    for (const auto& b : decode_blob_list(cbv(registrations, registrations_len))) {
      blockchainer::codec::CodecReader r(cbv(b.data(), b.size()));
      p.children.push_back(blockchainer::ChildRegistration::decode(r));
      r.assert_fully_consumed();
    }
    return bytes_result(p.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_genesis_commit_subchain_policy(const uint8_t* genesis, size_t genesis_len,
                                                                     const uint8_t* policy, size_t policy_len) {
  try {
    blockchainer::GenesisSpec g = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    blockchainer::SubchainPolicy p = blockchainer::SubchainPolicy::decode_all(cbv(policy, policy_len));
    if (std::string e = p.validate(g.chain_id); !e.empty()) return error_result(e.c_str());
    p.commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_genesis_commit_parent_binding(const uint8_t* genesis, size_t genesis_len,
                                                                    uint64_t parent_chain_id,
                                                                    const uint8_t* parent_genesis_hash) {
  try {
    if (!parent_genesis_hash) return error_result("binding: null parent genesis hash");
    blockchainer::GenesisSpec g = blockchainer::GenesisSpec::decode_all(cbv(genesis, genesis_len));
    blockchainer::ParentBinding{parent_chain_id, to_hash32(parent_genesis_hash)}.commit_to(g);
    return bytes_result(g.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_block_header(const uint8_t* block, size_t block_len) {
  try { return bytes_result(blockchainer::Block::decode_all(cbv(block, block_len)).header.encode()); }
  catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_header_subchain_root(const uint8_t* header, size_t header_len) {
  try {
    blockchainer::BlockHeader h = blockchainer::BlockHeader::decode_all(cbv(header, header_len));
    return bytes_result(h.subchain_root.data(), h.subchain_root.size());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_certified_header_encode(const uint8_t* header, size_t header_len,
                                                              const uint8_t* certificate, size_t certificate_len) {
  try {
    blockchainer::CertifiedHeader c{blockchainer::BlockHeader::decode_all(cbv(header, header_len)),
                                    blockchainer::crypto::Bytes(certificate, certificate + certificate_len)};
    return bytes_result(c.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_anchor_encode(uint64_t child_chain_id, const uint8_t* certified_headers,
                                                    size_t certified_headers_len) {
  try {
    blockchainer::Anchor a;
    a.child_chain_id = child_chain_id;
    for (const auto& b : decode_blob_list(cbv(certified_headers, certified_headers_len)))
      a.headers.push_back(blockchainer::CertifiedHeader::decode_all(cbv(b.data(), b.size())));
    return bytes_result(a.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_chain_propose_anchored(BlockchainerChain c, uint64_t timestamp,
                                                             const uint8_t* anchors, size_t anchors_len) {
  if (!c) return error_result("null chain");
  try {
    return bytes_result(as_chain(c)->engine->propose_anchored({}, timestamp, decode_anchors(cbv(anchors, anchors_len))).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_chain_certify(BlockchainerChain c, const uint8_t* block, size_t block_len) {
  if (!c) return error_result("null chain");
  try {
    auto* bft = dynamic_cast<blockchainer::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return error_result("chain: engine is not BFT (only BFT blocks carry commit certificates)");
    blockchainer::Block b = blockchainer::Block::decode_all(cbv(block, block_len));
    ChainBox* box = as_box(c);
    // Our index follows rotations (and we may have left the set).
    auto idx = bft->validators().index_of(box->key.public_key);
    if (!idx) return error_result("chain: this node is not in the current validator set");
    blockchainer::consensus::Vote vote = bft->precommit(b, *idx, box->key);
    auto cert = bft->try_commit(b, {vote});
    if (!cert) return error_result("chain: this validator alone does not reach quorum; gather the other precommits");
    return bytes_result(cert->encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

// ── Light client ──────────────────────────────────────────────────────────────

namespace {
blockchainer::client::LightClient* as_lc(BlockchainerLightClient lc) {
  return static_cast<blockchainer::client::LightClient*>(lc);
}
}  // namespace

BlockchainerLightClient blockchainer_light_client_new(uint64_t chain_id, const uint8_t* validators,
                                                      size_t validators_len, const uint8_t* trusted_header,
                                                      size_t header_len) {
  try {
    return new blockchainer::client::LightClient(
        chain_id, blockchainer::consensus::ValidatorSet::decode_all(cbv(validators, validators_len)),
        blockchainer::BlockHeader::decode_all(cbv(trusted_header, header_len)));
  } catch (...) { return nullptr; }
}

int blockchainer_light_client_accept(BlockchainerLightClient lc, const uint8_t* header, size_t header_len,
                                     const uint8_t* certificate, size_t certificate_len) {
  if (!lc) return 0;
  try {
    return as_lc(lc)->accept(blockchainer::BlockHeader::decode_all(cbv(header, header_len)),
                             blockchainer::consensus::CommitCertificate::decode_all(cbv(certificate, certificate_len))).ok ? 1 : 0;
  } catch (...) { return 0; }
}

int blockchainer_light_client_accept_skipping(BlockchainerLightClient lc, const uint8_t* header, size_t header_len,
                                              const uint8_t* certificate, size_t certificate_len,
                                              const uint8_t* validators, size_t validators_len) {
  if (!lc) return 0;
  try {
    blockchainer::client::LightClient::Result r = as_lc(lc)->accept_skipping(
        blockchainer::BlockHeader::decode_all(cbv(header, header_len)),
        blockchainer::consensus::CommitCertificate::decode_all(cbv(certificate, certificate_len)),
        blockchainer::consensus::ValidatorSet::decode_all(cbv(validators, validators_len)));
    if (r.ok) return 1;
    return r.insufficient_overlap ? 2 : 0;
  } catch (...) { return 0; }
}

uint64_t blockchainer_light_client_height(BlockchainerLightClient lc) { return lc ? as_lc(lc)->height() : 0; }

BlockchainerBufferResult blockchainer_light_client_tip(BlockchainerLightClient lc) {
  if (!lc) return error_result("null light client");
  return bytes_result(as_lc(lc)->tip().encode());
}

BlockchainerBufferResult blockchainer_light_client_validators(BlockchainerLightClient lc) {
  if (!lc) return error_result("null light client");
  return bytes_result(as_lc(lc)->validators().encode());
}

void blockchainer_light_client_set_trust_level(BlockchainerLightClient lc, uint32_t numerator, uint32_t denominator) {
  if (lc) as_lc(lc)->set_trust_level(numerator, denominator);
}
void blockchainer_light_client_set_max_skip(BlockchainerLightClient lc, uint64_t blocks) {
  if (lc) as_lc(lc)->set_max_skip(blocks);
}
void blockchainer_light_client_free(BlockchainerLightClient lc) { delete as_lc(lc); }

int blockchainer_chain_schedule_validators(BlockchainerChain c, const uint8_t* validator_pks,
                                           const uint64_t* validator_powers, size_t validator_count) {
  if (!c || !validator_pks || !validator_powers || validator_count == 0) return 0;
  try {
    auto* bft = dynamic_cast<blockchainer::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return 0;
    std::vector<blockchainer::consensus::Validator> vs;
    for (size_t i = 0; i < validator_count; ++i)
      vs.push_back(blockchainer::consensus::Validator{
          blockchainer::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1)), validator_powers[i]});
    bft->schedule_validator_set(blockchainer::consensus::ValidatorSet(std::move(vs)));  // throws if malformed
    return 1;
  } catch (...) { return 0; }
}

BlockchainerBufferResult blockchainer_chain_validators(BlockchainerChain c) {
  if (!c) return error_result("null chain");
  try {
    auto* bft = dynamic_cast<blockchainer::consensus::BftEngine*>(as_chain(c)->engine.get());
    if (!bft) return error_result("chain: engine is not BFT");
    return bytes_result(bft->validators().encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_chain_child_tip(BlockchainerChain c, uint64_t child_chain_id) {
  if (!c) return error_result("null chain");
  const blockchainer::SubchainRegistry* reg = as_chain(c)->engine->ledger().subchains();
  if (!reg) return error_result("chain: not a parent (no subchain policy)");
  const blockchainer::BlockHeader* tip = reg->tip_of(child_chain_id);
  if (!tip) return error_result("chain: child not registered");
  return bytes_result(tip->encode());
}

BlockchainerBufferResult blockchainer_chain_subchain_hop(BlockchainerChain c, uint64_t child_chain_id) {
  if (!c) return error_result("null chain");
  try {
    const blockchainer::SubchainRegistry* reg = as_chain(c)->engine->ledger().subchains();
    if (!reg) return error_result("chain: not a parent (no subchain policy)");
    return bytes_result(reg->hop(child_chain_id).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_chain_prove_account(BlockchainerChain c, const uint8_t* address) {
  if (!c) return error_result("null chain");
  try {
    auto* acc = dynamic_cast<const blockchainer::AccountStateMachine*>(&as_chain(c)->engine->ledger().state());
    if (!acc) return error_result("chain: not an account-model chain");
    return bytes_result(acc->prove_account(to_hash32(address)).encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_subchain_proof_encode(const uint8_t* hops, size_t hops_len,
                                                            const uint8_t* state_proof, size_t state_proof_len) {
  try {
    blockchainer::SubchainProof p;
    for (const auto& b : decode_blob_list(cbv(hops, hops_len)))
      p.hops.push_back(blockchainer::SubchainProof::Hop::decode_all(cbv(b.data(), b.size())));
    p.state_proof = blockchainer::StateProof::decode_all(cbv(state_proof, state_proof_len));
    return bytes_result(p.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

int blockchainer_subchain_proof_verify(const uint8_t* parent_subchain_root, const uint8_t* proof, size_t proof_len) {
  if (!parent_subchain_root || !proof) return 0;
  try {
    return blockchainer::verify_subchain_proof(to_hash32(parent_subchain_root),
                                               blockchainer::SubchainProof::decode_all(cbv(proof, proof_len))) ? 1 : 0;
  } catch (...) { return 0; }
}

// ── Chain tree ────────────────────────────────────────────────────────────────

namespace {
// The tree as seen from the C ABI: the hooks are C callbacks with a ctx.
class CallbackTree : public blockchainer::node::ChainTree {
 public:
  static std::unique_ptr<CallbackTree> build(const blockchainer::node::ChainTreeSpec& spec) {
    auto base = blockchainer::node::ChainTree::build(spec);
    auto t = std::unique_ptr<CallbackTree>(new CallbackTree());
    static_cast<blockchainer::node::ChainTree&>(*t) = std::move(*base);
    return t;
  }
  BlockchainerTxSourceFn tx_fn = nullptr; void* tx_ctx = nullptr;
  BlockchainerFinalizedFn fin_fn = nullptr; void* fin_ctx = nullptr;

 protected:
  std::vector<blockchainer::Transaction> select_txs(const Built& n, std::uint64_t ts) override {
    if (!tx_fn) return {};
    std::vector<std::uint8_t> buf(BLOCKCHAINER_MAX_TX_LIST);
    size_t len = 0;
    if (tx_fn(tx_ctx, n.chain_id, ts, buf.data(), buf.size(), &len) != 1) return {};
    if (len > buf.size()) throw std::runtime_error("tx source overflowed its buffer");
    std::vector<blockchainer::Transaction> out;
    for (const auto& b : decode_blob_list(cbv(buf.data(), len)))
      out.push_back(blockchainer::Transaction::decode_all(cbv(b.data(), b.size())));
    return out;
  }
  void on_finalized(const Built& n, const blockchainer::Block& b) override {
    if (!fin_fn) return;
    blockchainer::crypto::Bytes e = b.encode();
    fin_fn(fin_ctx, n.chain_id, e.data(), e.size());
  }
 private:
  CallbackTree() = default;
};
CallbackTree* as_tree(BlockchainerChainTree t) { return static_cast<CallbackTree*>(t); }
}  // namespace

BlockchainerBufferResult blockchainer_chain_tree_node_encode(
    uint64_t chain_id, const uint8_t* blueprint, size_t blueprint_len, uint16_t assurance_level,
    const uint8_t* account_addrs, const uint64_t* account_balances, size_t account_count,
    const uint8_t* validator_pks, const uint8_t* validator_secrets, const uint64_t* validator_powers,
    size_t validator_count, uint16_t max_anchors_per_block, uint16_t max_headers_per_anchor,
    const uint8_t* children, size_t children_len) {
  try {
    blockchainer::node::ChainTreeSpec::Node n;
    n.chain_id = chain_id;
    n.blueprint = blockchainer::ChainBlueprint::decode_all(cbv(blueprint, blueprint_len));
    n.assurance_level = assurance_level;
    if (account_count > 0 && (!account_addrs || !account_balances)) return error_result("tree node: null accounts");
    for (size_t i = 0; i < account_count; ++i)
      n.accounts.push_back(blockchainer::GenesisAccount{to_hash32(account_addrs + 32 * i),
                                                        static_cast<blockchainer::codec::u128>(account_balances[i])});
    if (validator_count > 0 && (!validator_pks || !validator_powers)) return error_result("tree node: null validators");
    for (size_t i = 0; i < validator_count; ++i) {
      blockchainer::node::ChainTreeSpec::Validator v;
      v.bls_public = blockchainer::crypto::Bytes(validator_pks + 48 * i, validator_pks + 48 * (i + 1));
      if (validator_secrets) {
        blockchainer::crypto::Bytes s(validator_secrets + 32 * i, validator_secrets + 32 * (i + 1));
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
        blockchainer::codec::CodecReader r(cbv(b.data(), b.size()));
        n.children.push_back(blockchainer::node::ChainTreeSpec::Node::decode(r));
        r.assert_fully_consumed();
      }
    }
    return bytes_result(n.encode());
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_chain_tree_spec_error(const uint8_t* spec, size_t spec_len) {
  try {
    std::string e = blockchainer::node::ChainTreeSpec::decode_all(cbv(spec, spec_len)).validate();
    return bytes_result(reinterpret_cast<const std::uint8_t*>(e.data()), e.size());
  } catch (const std::exception& e) {
    std::string m = std::string("malformed spec: ") + e.what();
    return bytes_result(reinterpret_cast<const std::uint8_t*>(m.data()), m.size());
  }
}

BlockchainerChainTree blockchainer_chain_tree_build(const uint8_t* spec, size_t spec_len) {
  try {
    return CallbackTree::build(blockchainer::node::ChainTreeSpec::decode_all(cbv(spec, spec_len))).release();
  } catch (...) { return nullptr; }
}

BlockchainerBufferResult blockchainer_chain_tree_chain_ids(BlockchainerChainTree t) {
  if (!t) return error_result("null tree");
  blockchainer::codec::CodecWriter w;
  w.write_list(as_tree(t)->chain_ids(), [](blockchainer::codec::CodecWriter& ww, std::uint64_t id) { ww.write_u64(id); });
  return bytes_result(w.take());
}
uint64_t blockchainer_chain_tree_height(BlockchainerChainTree t, uint64_t chain_id) {
  if (!t || !as_tree(t)->has(chain_id)) return 0;
  return as_tree(t)->height(chain_id);
}
BlockchainerBufferResult blockchainer_chain_tree_genesis(BlockchainerChainTree t, uint64_t chain_id) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->node(chain_id).genesis.encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_chain_tree_tip(BlockchainerChainTree t, uint64_t chain_id) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->node(chain_id).chain.engine->tip().encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
uint32_t blockchainer_chain_tree_pending_anchors(BlockchainerChainTree t, uint64_t chain_id) {
  if (!t || !as_tree(t)->has(chain_id)) return 0;
  return static_cast<uint32_t>(as_tree(t)->pending_anchors_for(chain_id).size());
}
BlockchainerBufferResult blockchainer_chain_tree_produce(BlockchainerChainTree t, uint64_t chain_id, uint64_t timestamp) {
  if (!t) return error_result("null tree");
  try { return bytes_result(as_tree(t)->produce(chain_id, timestamp).encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_chain_tree_prove_account(BlockchainerChainTree t, uint64_t chain_id, const uint8_t* address) {
  if (!t) return error_result("null tree");
  if (!address) return error_result("null address");
  try { return bytes_result(as_tree(t)->prove_account(chain_id, to_hash32(address)).encode()); } catch (const std::exception& e) { return error_result(e.what()); }
}
int blockchainer_chain_tree_verify(BlockchainerChainTree t, const uint8_t* proof, size_t proof_len) {
  if (!t || !proof) return 0;
  try { return as_tree(t)->verify(blockchainer::SubchainProof::decode_all(cbv(proof, proof_len))) ? 1 : 0; } catch (...) { return 0; }
}
void blockchainer_chain_tree_set_tx_source(BlockchainerChainTree t, BlockchainerTxSourceFn fn, void* ctx) {
  if (t) { as_tree(t)->tx_fn = fn; as_tree(t)->tx_ctx = ctx; }
}
void blockchainer_chain_tree_set_finalized_hook(BlockchainerChainTree t, BlockchainerFinalizedFn fn, void* ctx) {
  if (t) { as_tree(t)->fin_fn = fn; as_tree(t)->fin_ctx = ctx; }
}
void blockchainer_chain_tree_free(BlockchainerChainTree t) { delete as_tree(t); }

// ── Foreign components (C callbacks → registry) ───────────────────────────────

int blockchainer_registry_add_engine(const char* id, BlockchainerJustifyFn justify,
                                     BlockchainerVerifyFn verify, void* ctx, int grind_proof) {
  if (!id || !*id || !justify || !verify) return 0;
  try {
    blockchainer::node::register_foreign_engine(
        id,
        [justify, ctx](const blockchainer::Block& b) -> blockchainer::crypto::Bytes {
          const blockchainer::crypto::Bytes enc = b.encode();
          std::vector<std::uint8_t> out(BLOCKCHAINER_MAX_JUSTIFICATION);
          size_t n = 0;
          if (justify(ctx, enc.data(), enc.size(), out.data(), out.size(), &n) != 1 || n > out.size())
            throw std::runtime_error("foreign justify refused");
          out.resize(n);
          return out;
        },
        [verify, ctx](const blockchainer::Block& b, blockchainer::crypto::ByteView j) -> bool {
          const blockchainer::crypto::Bytes enc = b.encode();
          return verify(ctx, enc.data(), enc.size(), j.data(), j.size()) == 1;
        },
        grind_proof != 0);
    return 1;
  } catch (...) {
    return 0;
  }
}

int blockchainer_registry_add_proposer(const char* id, BlockchainerProposerFn fn, void* ctx) {
  if (!id || !*id || !fn) return 0;
  try {
    blockchainer::node::register_foreign_proposer(
        id, [fn, ctx](std::uint64_t height, std::uint32_t n) { return fn(ctx, height, n); });
    return 1;
  } catch (...) {
    return 0;
  }
}

// ── Interop addresses ─────────────────────────────────────────────────────────

BlockchainerBufferResult blockchainer_eth_address(const uint8_t* secret32) {
  try {
    auto a = blockchainer::interop::evm::address_from_secret(blockchainer::codec::ByteView(secret32, 32));
    return string_result(blockchainer::interop::evm::to_checksum_address(a));
  } catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_btc_address(const uint8_t* secret32) {
  try {
    return string_result(blockchainer::interop::bitcoin::p2wpkh_address_from_secret(
        blockchainer::codec::ByteView(secret32, 32)));
  } catch (const std::exception& e) { return error_result(e.what()); }
}
BlockchainerBufferResult blockchainer_sol_address(const uint8_t* ed_secret64) {
  try {
    blockchainer::interop::solana::Pubkey pk{};
    std::memcpy(pk.data(), ed_secret64 + 32, 32);  // libsodium sk = seed‖pub
    return string_result(blockchainer::interop::solana::to_address(pk));
  } catch (const std::exception& e) { return error_result(e.what()); }
}

// ── EVM smart-contract interaction ────────────────────────────────────────────

BlockchainerBufferResult blockchainer_eth_erc20_transfer(const uint8_t* to20, uint64_t amount) {
  try {
    namespace abi = blockchainer::interop::abi;
    blockchainer::interop::evm::Address to{};
    std::memcpy(to.data(), to20, 20);
    auto data = abi::encode_call("transfer(address,uint256)",
                                 {abi::encode_address(to),
                                  abi::encode_uint(static_cast<blockchainer::codec::u128>(amount))});
    return bytes_result(data);
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_eth_selector(const char* signature) {
  try {
    return bytes_result(blockchainer::interop::abi::selector(signature));
  } catch (const std::exception& e) { return error_result(e.what()); }
}

BlockchainerBufferResult blockchainer_eth_sign_legacy(
    uint64_t nonce, uint64_t gas_price, uint64_t gas_limit, const uint8_t* to20,
    int has_to, uint64_t value, const uint8_t* data, size_t data_len,
    uint64_t chain_id, const uint8_t* secret32) {
  try {
    blockchainer::interop::evm::LegacyTx tx;
    tx.nonce = nonce;
    tx.gas_price = static_cast<blockchainer::codec::u128>(gas_price);
    tx.gas_limit = gas_limit;
    if (has_to) {
      blockchainer::interop::evm::Address to{};
      std::memcpy(to.data(), to20, 20);
      tx.to = to;
    }
    tx.value = static_cast<blockchainer::codec::u128>(value);
    if (data != nullptr && data_len > 0) tx.data.assign(data, data + data_len);
    tx.chain_id = chain_id;
    auto st = blockchainer::interop::evm::sign(tx, blockchainer::codec::ByteView(secret32, 32));
    return bytes_result(st.raw);
  } catch (const std::exception& e) { return error_result(e.what()); }
}

void blockchainer_buffer_free(BlockchainerBuffer* buf) {
  if (buf != nullptr && buf->data != nullptr) {
    std::free(buf->data);
    buf->data = nullptr;
    buf->len = 0;
  }
}

void blockchainer_str_free(char* str) { std::free(str); }

}  // extern "C"
