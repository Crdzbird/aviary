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

/** Build, sign, and submit a transfer carrying a priority-fee [fee] (a version-2
 *  envelope). [secret] is the 64-byte Ed25519 key. Returns the txid or an error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_submit_transfer_fee(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce, uint64_t fee);

/** Generate a fresh HYBRID post-quantum account (Ed25519 + ML-DSA-65). Returns a
 *  buffer laid out as [pub_len: 4 bytes big-endian][public_key][secret_key]. The
 *  account address is BLAKE3-256(public_key). Free with blockchainer_buffer_free. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_new_account_hybrid(BlockchainerNode node);

/** Build, sign, and submit a HYBRID (post-quantum) transfer. [secret]/[public_key]
 *  are the hybrid key blobs from blockchainer_node_new_account_hybrid. Returns the
 *  txid (32 bytes) or an error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_node_submit_transfer_hybrid(
    BlockchainerNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* public_key, size_t pk_len,
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

/* ── Custodial key store (S3) ──────────────────────────────────────────────────
 * A container that seals account signing secrets at rest. A node never needs to
 * hold a plaintext key: create a store under a passphrase, add accounts, persist
 * the (encrypted) blob, reload it LOCKED, unlock, then reveal a secret only to
 * sign. Each secret is bound to its account address as AAD. */

typedef void* BlockchainerKeyStore;

/** Create a new key store protected by [passphrase] (returned unlocked). NULL on failure. */
BLOCKCHAINER_API BlockchainerKeyStore blockchainer_keystore_new(const char* passphrase);

/** Seal an account's secret; writes its 32-byte address into [out_address] (>=32 bytes).
 *  Returns 0 on success, nonzero on failure (e.g. store locked). */
BLOCKCHAINER_API int blockchainer_keystore_add_account(
    BlockchainerKeyStore ks, const uint8_t* public_key, size_t pk_len,
    const uint8_t* secret, size_t sk_len, uint8_t* out_address);

/** Reveal the sealed secret for [address] (32 bytes). Requires unlocked. Returns
 *  the secret buffer or an error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_keystore_reveal(
    BlockchainerKeyStore ks, const uint8_t* address);

/** Serialise the store (keyring envelope + sealed entries; no plaintext key). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_keystore_serialise(BlockchainerKeyStore ks);

/** Load a serialised store. Comes back LOCKED — call unlock before revealing. NULL on error. */
BLOCKCHAINER_API BlockchainerKeyStore blockchainer_keystore_load(const uint8_t* blob, size_t len);

/** Unlock with a passphrase. Returns 0 on success, nonzero on wrong passphrase. */
BLOCKCHAINER_API int blockchainer_keystore_unlock(BlockchainerKeyStore ks, const char* passphrase);

/** 1 if currently unlocked, else 0. */
BLOCKCHAINER_API int blockchainer_keystore_unlocked(BlockchainerKeyStore ks);

/** Destroy a key-store handle. */
BLOCKCHAINER_API void blockchainer_keystore_free(BlockchainerKeyStore ks);

/* ── Portable account export (tiered encrypted backup) ─────────────────────────
 * Move one signing account off the node under a chosen trust tier, all fail-
 * closed. An account is a (public_key, secret) pair; the "open" calls return it
 * as a self-describing blob: [pub_len:4 BE][public_key][secret] (the same layout
 * blockchainer_node_new_account_hybrid uses), so a binding unpacks it once and
 * reuses that helper. */

/** A hybrid KEM keypair (X25519 + ML-KEM-768) — a "device key" for the
 *  recipient-bound tiers. Returns [pub_len:4 BE][public][secret], or an error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_hybrid_kem_keygen(void);

/** Tier 1 — passphrase (MolecularVault + Argon2id). ops/mem are Argon2id work
 *  factors; pass 0,0 for the SENSITIVE preset. Returns the sealed envelope. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_seal_passphrase(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const char* passphrase, uint64_t ops, size_t mem);
/** Open a passphrase envelope → [pub_len:4 BE][public][secret]. Error on wrong
 *  passphrase / tamper. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_open_passphrase(
    const uint8_t* envelope, size_t env_len, const char* passphrase);

/** Tier 2 — recipient-bound (PQ). Seal to a device's hybrid-KEM public key; only
 *  its KEM secret opens it. No shared secret to phish. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_seal_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len);
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_open_to(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len);

/** Tier 3 — signed-recipient (Fortress-grade): recipient-bound AND authenticated.
 *  The receiver verifies the owner's hybrid signature on open. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_seal_signed_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len,
    const uint8_t* sender_sig_secret, size_t ss_len);
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_portable_open_signed_from(
    const uint8_t* envelope, size_t env_len,
    const uint8_t* recipient_kem_secret, size_t rk_len,
    const uint8_t* sender_sig_public, size_t sp_len);

/* ── Threshold accounts (FROST t-of-n; the key never exists) ───────────────────
 * A t-of-n account whose secret key is never assembled: dealt shares sign in a
 * two-round ceremony, and the aggregate is a STANDARD 64-byte Ed25519 signature
 * — so a threshold-signed transaction verifies through the ordinary Ed25519 path
 * (the chain can't tell). Flow: create() deals n share cards; each participating
 * card runs commit() (keep the nonces, broadcast the commitment); a coordinator
 * opens a ceremony over the fixed roster of commitments; each card signs; the
 * shares aggregate into the group signature.
 *
 * Blob layouts (bindings split these; all multi-byte ints big-endian):
 *   create()   → [u16 count]  then count × ( [u32 len][share-card bytes] )
 *   commit()   → [hiding_nonce 32][binding_nonce 32][commitment 66]
 *                where commitment = [id 2][hiding_commit 32][binding_commit 32].
 *                Keep bytes[0..64) (the secret nonces); broadcast bytes[64..130)
 *                (the commitment) to the coordinator.
 *   a roster   = the participating commitments (66 bytes each) concatenated. */

/** Opaque signing-ceremony handle (fixed message + roster). */
typedef void* BlockchainerCeremony;

/** Deal a t-of-n account: returns the packed share cards (see layout above). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_create(uint16_t n, uint16_t t);

/** A card's identity fields as a 66-byte blob: [id u16 BE][group_pk 32][public
 *  share 32]. The group public key is the account's on-chain author key. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_share_info(
    const uint8_t* card, size_t card_len);

/** Seal a share card for rest (MolecularVault + Argon2id; ops=mem=0 → SENSITIVE).
 *  The envelope binds the group key + slot, so it can't be relocated. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_share_seal(
    const uint8_t* card, size_t card_len, const char* passphrase, uint64_t ops, size_t mem);
/** Open a sealed share card → the card bytes. Fails closed on wrong passphrase. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_share_unseal(
    const uint8_t* envelope, size_t env_len, const char* passphrase);

/** Round 1: fresh nonces + commitment for a card (see commit() layout above). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_commit(
    const uint8_t* card, size_t card_len);

/** Open a ceremony over [message] and a [commitments] roster (66 bytes each,
 *  concatenated). Returns a handle, or NULL on malformed input. */
BLOCKCHAINER_API BlockchainerCeremony blockchainer_threshold_ceremony_new(
    const uint8_t* group_pk32, const uint8_t* message, size_t msg_len,
    const uint8_t* commitments, size_t commits_len);
BLOCKCHAINER_API void blockchainer_threshold_ceremony_free(BlockchainerCeremony c);

/** Round 2: this card's 32-byte signature share, using its round-1 [nonces64]. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_sign(
    BlockchainerCeremony c, const uint8_t* card, size_t card_len, const uint8_t* nonces64);

/** Attribute a share to participant [id] (public share 32 B): 1 valid, 0 invalid. */
BLOCKCHAINER_API int blockchainer_threshold_verify_share(
    BlockchainerCeremony c, uint16_t id, const uint8_t* public32, const uint8_t* sig_share32);

/** Aggregate [count] (id, 32-byte share) pairs → a 64-byte Ed25519 signature.
 *  The pairs may be in any order; the ceremony reorders them internally. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_threshold_aggregate(
    BlockchainerCeremony c, const uint16_t* ids, const uint8_t* sig_shares, size_t count);

/* ── AssuranceProfile (one security posture, describing every tier) ────────────
 * A named posture fixes a consistent tier at every seam. Levels: 1 = standard,
 * 2 = hardened, 3 = fortress. `describe` returns a fixed blob:
 *   [transport u8 (0=mutual, 1=ratcheted)]
 *   [consensus u8 (0=stake-weighted, 1=vrf)]
 *   [threshold_custody u8 (0/1)]
 *   [sealed_mempool  u8 (0/1)]
 *   [sig_algo_count  u8][mandated SigAlgo bytes ...]
 * so a consumer can wire the classes the posture requires. Unknown level → error. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_assurance_describe(uint16_t level);

/* ── Transaction encoding ──────────────────────────────────────────────────────
 * Encode a transaction from its fields into the canonical wire form (e.g. to
 * seal into the mempool below). [fee] is only carried when version >= 2. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_tx_encode(
    uint8_t version, uint64_t chain_id, uint8_t sig_algo, uint64_t fee,
    const uint8_t* body, size_t body_len, const uint8_t* author, size_t author_len,
    const uint8_t* signature, size_t sig_len);

/* ── SealedMempool (anti-MEV: transactions invisible until inclusion) ──────────
 * A submitter seals an encoded transaction to the block proposer's hybrid-KEM
 * key; the pool orders by a cleartext fee hint WITHOUT decrypting, and only the
 * proposer opens the winners at build time. A front-runner sees only ciphertext.
 * Submission-only: committed blocks carry ordinary plaintext transactions. */

/** Opaque sealed-mempool handle. */
typedef void* BlockchainerSealedMempool;

/** Seal an encoded transaction to [proposer_kem_public] → a SealedTx blob (its
 *  cleartext fee hint is the transaction's declared fee). */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_sealed_tx_seal(
    const uint8_t* tx_encoded, size_t tx_len, const uint8_t* proposer_kem_public, size_t pk_len);

BLOCKCHAINER_API BlockchainerSealedMempool blockchainer_sealed_mempool_new(size_t capacity);
/** Admit a SealedTx (no decryption). Returns 1 if admitted, 0 if full/rejected. */
BLOCKCHAINER_API int blockchainer_sealed_mempool_add(
    BlockchainerSealedMempool m, const uint8_t* sealed_tx, size_t len);
BLOCKCHAINER_API size_t blockchainer_sealed_mempool_size(BlockchainerSealedMempool m);
/** Proposer-only: open up to [max] highest-fee submissions with the KEM secret.
 *  Returns a blob: [count u32] then count × ( [tx_len u32][encoded tx] ). Entries
 *  that don't decrypt / decode, or whose real fee != the committed hint, are
 *  silently skipped. */
BLOCKCHAINER_API BlockchainerBufferResult blockchainer_sealed_mempool_reveal(
    BlockchainerSealedMempool m, const uint8_t* proposer_kem_secret, size_t sk_len, size_t max);
BLOCKCHAINER_API void blockchainer_sealed_mempool_free(BlockchainerSealedMempool m);

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
