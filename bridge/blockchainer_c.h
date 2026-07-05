/**
 * Blockchainer C ABI
 *
 * Flat C API wrapping the C++ blockchainer core, for consumption by Dart
 * (dart:ffi), Go (cgo), and any language with C FFI support. Mirrors the
 * conventions of cryptolib's C ABI.
 *
 * Memory contract:
 *   - Functions returning BlockchainerBuffer allocate via malloc().
 *     The caller MUST call blockchainer_buffer_free() when done.
 *   - Input data is borrowed (const pointers) — never freed by the library.
 *   - Error strings are heap-allocated — free with blockchainer_str_free().
 *   - Version strings are static — do NOT free.
 *
 * Boundary contract (cpp-crypto-engineer): all C++ exceptions are isolated at
 * this boundary and surfaced as error returns; no exception crosses into C.
 */
#ifndef BLOCKCHAINER_C_H
#define BLOCKCHAINER_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef BLOCKCHAINER_BUILD_SHARED
#    define BLOCKCHAINER_API __declspec(dllexport)
#  else
#    define BLOCKCHAINER_API __declspec(dllimport)
#  endif
#else
#  define BLOCKCHAINER_API __attribute__((visibility("default")))
#endif

/** Stable status codes returned by int-returning functions. Values are ABI —
 *  never renumber. Bindings surface these as typed errors so failures are
 *  distinguishable (a silently-swallowed credit is a fund-loss bug). */
typedef enum {
  BLOCKCHAINER_OK = 0,
  /** Generic failure (exception with no more specific mapping). */
  BLOCKCHAINER_ERR = 1,
  /** Genesis is sealed (a block was produced) — credit is no longer valid;
   *  fund accounts with transfers instead. */
  BLOCKCHAINER_ERR_GENESIS_SEALED = 2,
  /** Malformed input (wrong length, null where required, bad encoding). */
  BLOCKCHAINER_ERR_BAD_INPUT = 3
} BlockchainerStatus;

/** Heap-allocated byte buffer. data is NULL on error. */
typedef struct {
  uint8_t* data;
  size_t   len;
} BlockchainerBuffer;

/** A buffer plus a possible error string (NULL on success). */
typedef struct {
  BlockchainerBuffer buf;
  char*              error;
} BlockchainerBufferResult;

/** Initialize the crypto backend. Returns 0 on success, non-zero on failure. */
BLOCKCHAINER_API int blockchainer_init(void);

/** Blockchainer library version. Static string — do not free. */
BLOCKCHAINER_API const char* blockchainer_version(void);

/** Linked cryptolib backend version (proves linkage). Static — do not free. */
BLOCKCHAINER_API const char* blockchainer_crypto_backend_version(void);

/** BLAKE3-256 of [msg, msg+len). On success buf.len == 32. Free with
 *  blockchainer_buffer_free(). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_hash(const uint8_t* msg,
                                                            size_t len);

/** Canonical transaction id (32 bytes) for the given fields:
 *  BLAKE3-256( 0x03 ‖ canonical(tx) ). Proves the canonical tx encoding is
 *  reproducible from any binding. Free buf with blockchainer_buffer_free(). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_txid(
    uint8_t version, uint64_t chain_id,
    const uint8_t* body, size_t body_len,
    const uint8_t* author, size_t author_len,
    const uint8_t* signature, size_t sig_len);

/** Canonical block-header hash (32 bytes): BLAKE3-256( 0x02 ‖ canonical(header) ).
 *  parent_hash, tx_root and state_root must each point to 32 bytes. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_header_hash(
    uint8_t version, uint64_t chain_id, uint64_t height,
    const uint8_t* parent_hash, uint64_t timestamp,
    const uint8_t* tx_root, const uint8_t* state_root,
    const uint8_t* consensus_data, size_t consensus_len);

/* ── Verification (light client) ──────────────────────────────────────────────
 * Blob-based: inputs are canonical encodings produced by the C++ core. Each
 * returns 1 if valid, 0 otherwise (a malformed blob returns 0, never crashes). */

/** Verify a state-inclusion proof against a 32-byte state_root. */
BLOCKCHAINER_API int blockchainer_verify_state_proof(
    const uint8_t* state_root, const uint8_t* proof, size_t proof_len);

/** Verify a transaction-inclusion proof against a 32-byte tx_root. */
BLOCKCHAINER_API int blockchainer_verify_tx_inclusion(
    const uint8_t* tx_root, const uint8_t* proof, size_t proof_len);

/** Verify a commit certificate against an encoded validator set. */
BLOCKCHAINER_API int blockchainer_verify_commit(
    uint64_t chain_id, const uint8_t* validator_set, size_t vset_len,
    const uint8_t* cert, size_t cert_len);

/* ── Demo node (full chain: ledger + BFT + mempool + observability) ───────────
 * A self-contained single-validator account-model node. The whole stack behind
 * a handle, for the example apps. Buffers are malloc'd; free with
 * blockchainer_buffer_free / blockchainer_str_free. */

typedef void* BlockchainerNode;

/** Create a node for [chain_id]. NULL on failure. */
BLOCKCHAINER_API BlockchainerNode blockchainer_node_new(uint64_t chain_id);
/** Destroy a node. */
BLOCKCHAINER_API void blockchainer_node_free(BlockchainerNode node);

/** Generate a fresh Ed25519 account: returns the 64-byte secret key (the public
 *  key is its last 32 bytes). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_new_account(BlockchainerNode node);
/** Account address (32 bytes) = BLAKE3-256(public key). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_account_address(
    const uint8_t* public_key, size_t pk_len);

/** Credit [address] (32 bytes) at genesis. Valid until the first block is
 *  produced; reads do NOT seal genesis. Returns a BlockchainerStatus:
 *  BLOCKCHAINER_OK, or BLOCKCHAINER_ERR_GENESIS_SEALED after the first block. */
BLOCKCHAINER_API int blockchainer_node_credit(BlockchainerNode node,
                                              const uint8_t* address, uint64_t amount);
/** Build, sign, and submit a transfer. [secret] is the 64-byte Ed25519 key.
 *  Returns the txid (32 bytes) or an error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_submit_transfer(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce);
/** Produce + finalize a block. Writes the new height to *out_height. Returns 0 on success. */
BLOCKCHAINER_API int blockchainer_node_produce_block(BlockchainerNode node,
                                                     uint64_t timestamp, uint64_t* out_height);

BLOCKCHAINER_API uint64_t blockchainer_node_height(BlockchainerNode node);
BLOCKCHAINER_API uint64_t blockchainer_node_balance(BlockchainerNode node, const uint8_t* address);
BLOCKCHAINER_API uint64_t blockchainer_node_nonce(BlockchainerNode node, const uint8_t* address);
BLOCKCHAINER_API uint64_t blockchainer_node_mempool_size(BlockchainerNode node);
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_state_root(BlockchainerNode node);
/** Drain collected chain events as a JSON array string (the streaming surface). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_drain_events(BlockchainerNode node);
/** Prometheus-format metrics snapshot. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_metrics(BlockchainerNode node);
/** State-inclusion proof for [address]; verify with blockchainer_verify_state_proof. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_prove_account(
    BlockchainerNode node, const uint8_t* address);

/* ── Interop address derivation (showcase) ─────────────────────────────────── */

/** Ethereum EIP-55 checksum address (string) from a 32-byte secp256k1 secret. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_eth_address(const uint8_t* secret32);
/** Bitcoin P2WPKH (bech32) address (string) from a 32-byte secp256k1 secret. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_btc_address(const uint8_t* secret32);
/** Solana address (base58 string) from a 64-byte Ed25519 secret. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_sol_address(const uint8_t* ed_secret64);

/* ── EVM smart-contract interaction ────────────────────────────────────────── */

/** ABI-encoded calldata for ERC-20 transfer(address,uint256): to is 20 bytes. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_eth_erc20_transfer(
    const uint8_t* to20, uint64_t amount);

/** ABI 4-byte function selector for a signature, e.g. "balanceOf(address)". */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_eth_selector(const char* signature);

/** Sign an EVM legacy (EIP-155) transaction with a 32-byte secp256k1 key.
 *  to20 may be NULL for contract creation (pass has_to=0). data may be NULL.
 *  Returns the raw signed transaction (RLP), ready for eth_sendRawTransaction. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_eth_sign_legacy(
    uint64_t nonce, uint64_t gas_price, uint64_t gas_limit,
    const uint8_t* to20, int has_to, uint64_t value,
    const uint8_t* data, size_t data_len, uint64_t chain_id,
    const uint8_t* secret32);

/** Free a buffer returned by this library. Safe on a zeroed/NULL buffer. */
BLOCKCHAINER_API void blockchainer_buffer_free(BlockchainerBuffer* buf);

/** Free an error string returned by this library. */
BLOCKCHAINER_API void blockchainer_str_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  /* BLOCKCHAINER_C_H */
