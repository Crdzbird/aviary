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
#include "blockchainer/node/demo_node.hpp"
#include "blockchainer/proofs.hpp"
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
