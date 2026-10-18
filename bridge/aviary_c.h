/**
 * Aviary C ABI
 *
 * Flat C API wrapping the C++ aviary core, for consumption by Dart
 * (dart:ffi), Go (cgo), and any language with C FFI support. Mirrors the
 * conventions of cryptolib's C ABI.
 *
 * Memory contract:
 *  - Functions returning AviaryBuffer allocate via malloc().
 *     The caller MUST call aviary_buffer_free() when done.
 *  - Input data is borrowed (const pointers) - never freed by the library.
 *  - Error strings are heap-allocated - free with aviary_str_free().
 *  - Version strings are static - do NOT free.
 *
 * Boundary contract (cpp-crypto-engineer): all C++ exceptions are isolated at
 * this boundary and surfaced as error returns; no exception crosses into C.
 *
 * Threading contract:
 *  - Every handle (node, chain, chain tree, networked node, light client,
 *    key store, sessions, address book, gates, mempools) is single threaded:
 *    one thread at a time may call into a given handle. Different handles may
 *    be used from different threads concurrently.
 *  - Hooks (event hooks, the chain tree's finalized and transaction source
 *    hooks, foreign engines and proposers) run synchronously on the thread
 *    that made the call which triggered them, before that call returns. A
 *    hook must not call back into the handle that invoked it.
 *  - The component registry and library initialisation are thread safe.
 */
#ifndef AVIARY_C_H
#define AVIARY_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#  ifdef AVIARY_BUILD_SHARED
#    define AVIARY_API __declspec(dllexport)
#  else
#    define AVIARY_API __declspec(dllimport)
#  endif
#else
#  define AVIARY_API __attribute__((visibility("default")))
#endif

/** Stable status codes returned by int-returning functions. Values are ABI - 
 *  never renumber. Bindings surface these as typed errors so failures are
 *  distinguishable (a silently-swallowed credit is a fund-loss bug). */
typedef enum {
  AVIARY_OK = 0,
  /** Generic failure (exception with no more specific mapping). */
  AVIARY_ERR = 1,
  /** Genesis is sealed (a block was produced) - credit is no longer valid;
   *  fund accounts with transfers instead. */
  AVIARY_ERR_GENESIS_SEALED = 2,
  /** Malformed input (wrong length, null where required, bad encoding). */
  AVIARY_ERR_BAD_INPUT = 3
} AviaryStatus;

/** Heap-allocated byte buffer. data is NULL on error. */
typedef struct {
  uint8_t* data;
  size_t   len;
} AviaryBuffer;

/** A buffer plus a possible error string (NULL on success). */
typedef struct {
  AviaryBuffer buf;
  char*              error;
} AviaryBufferResult;

/** Initialize the crypto backend. Returns 0 on success, non-zero on failure. */
AVIARY_API int aviary_init(void);

/** Aviary library version. Static string - do not free. */
AVIARY_API const char* aviary_version(void);

/** Linked cryptolib backend version (proves linkage). Static - do not free. */
AVIARY_API const char* aviary_crypto_backend_version(void);
/** The file system path of the loaded Aviary library, written to [out] (not NUL
 *  terminated). Returns the length, or 0 when unknown or [cap] is too small.
 *  Lets a binding that was handed this library as a bundled asset reopen it
 *  by path for symbol lookup. */
AVIARY_API size_t aviary_library_path(char* out, size_t cap);

/** BLAKE3-256 of [msg, msg+len). On success buf.len == 32. Free with
 *  aviary_buffer_free(). */
AVIARY_API AviaryBufferResult aviary_hash(const uint8_t* msg,
                                                            size_t len);

/** Canonical transaction id (32 bytes) for the given fields:
 *  BLAKE3-256( 0x03 || canonical(tx) ). Proves the canonical tx encoding is
 *  reproducible from any binding. Free buf with aviary_buffer_free(). */
AVIARY_API AviaryBufferResult aviary_txid(
    uint8_t version, uint64_t chain_id,
    const uint8_t* body, size_t body_len,
    const uint8_t* author, size_t author_len,
    const uint8_t* signature, size_t sig_len);

/** Canonical block-header hash (32 bytes): BLAKE3-256( 0x02 || canonical(header) ).
 *  parent_hash, tx_root and state_root must each point to 32 bytes. */
AVIARY_API AviaryBufferResult aviary_header_hash(
    uint8_t version, uint64_t chain_id, uint64_t height,
    const uint8_t* parent_hash, uint64_t timestamp,
    const uint8_t* tx_root, const uint8_t* state_root,
    const uint8_t* consensus_data, size_t consensus_len);

/* -- Verification (light client) ----------------------------------------------
 * Blob-based: inputs are canonical encodings produced by the C++ core. Each
 * returns 1 if valid, 0 otherwise (a malformed blob returns 0, never crashes). */

/** Verify a state-inclusion proof against a 32-byte state_root. */
AVIARY_API int aviary_verify_state_proof(
    const uint8_t* state_root, const uint8_t* proof, size_t proof_len);

/** Verify a transaction-inclusion proof against a 32-byte tx_root. */
AVIARY_API int aviary_verify_tx_inclusion(
    const uint8_t* tx_root, const uint8_t* proof, size_t proof_len);

/** Verify a commit certificate against an encoded validator set. */
AVIARY_API int aviary_verify_commit(
    uint64_t chain_id, const uint8_t* validator_set, size_t vset_len,
    const uint8_t* cert, size_t cert_len);

/* -- Demo node (full chain: ledger + BFT + mempool + observability) -----------
 * A self-contained single-validator account-model node. The whole stack behind
 * a handle, for the example apps. Buffers are malloc'd; free with
 * aviary_buffer_free / aviary_str_free. */

typedef void* AviaryNode;

/** Create a node for [chain_id]. NULL on failure. */
AVIARY_API AviaryNode aviary_node_new(uint64_t chain_id);
/** Destroy a node. */
AVIARY_API void aviary_node_free(AviaryNode node);

/** Generate a fresh Ed25519 account: returns the 64-byte secret key (the public
 *  key is its last 32 bytes). */
AVIARY_API AviaryBufferResult aviary_node_new_account(AviaryNode node);
/** Account address (32 bytes) = BLAKE3-256(public key). */
AVIARY_API AviaryBufferResult aviary_account_address(
    const uint8_t* public_key, size_t pk_len);

/** Credit [address] (32 bytes) at genesis. Valid until the first block is
 *  produced; reads do NOT seal genesis. Returns a AviaryStatus:
 *  AVIARY_OK, or AVIARY_ERR_GENESIS_SEALED after the first block. */
AVIARY_API int aviary_node_credit(AviaryNode node,
                                              const uint8_t* address, uint64_t amount);
/** Build, sign, and submit a transfer. [secret] is the 64-byte Ed25519 key.
 *  Returns the txid (32 bytes) or an error. */
AVIARY_API AviaryBufferResult aviary_node_submit_transfer(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce);

/** Build, sign, and submit a transfer carrying a priority-fee [fee] (a version-2
 *  envelope). [secret] is the 64-byte Ed25519 key. Returns the txid or an error. */
AVIARY_API AviaryBufferResult aviary_node_submit_transfer_fee(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce, uint64_t fee);

/** Generate a fresh HYBRID post-quantum account (Ed25519 + ML-DSA-65). Returns a
 *  buffer laid out as [pub_len: 4 bytes big-endian][public_key][secret_key]. The
 *  account address is BLAKE3-256(public_key). Free with aviary_buffer_free. */
AVIARY_API AviaryBufferResult aviary_node_new_account_hybrid(AviaryNode node);

/** Build, sign, and submit a HYBRID (post-quantum) transfer. [secret]/[public_key]
 *  are the hybrid key blobs from aviary_node_new_account_hybrid. Returns the
 *  txid (32 bytes) or an error. */
AVIARY_API AviaryBufferResult aviary_node_submit_transfer_hybrid(
    AviaryNode node, const uint8_t* secret, size_t sk_len,
    const uint8_t* public_key, size_t pk_len,
    const uint8_t* to_address, uint64_t amount, uint64_t nonce);
/** Produce + finalize a block. Writes the new height to *out_height. Returns 0 on success. */
AVIARY_API int aviary_node_produce_block(AviaryNode node,
                                                     uint64_t timestamp, uint64_t* out_height);

AVIARY_API uint64_t aviary_node_height(AviaryNode node);
AVIARY_API uint64_t aviary_node_balance(AviaryNode node, const uint8_t* address);
AVIARY_API uint64_t aviary_node_nonce(AviaryNode node, const uint8_t* address);
/** The nonce the next transfer from [address] must carry: the account nonce
 *  advanced past every transfer from it already pending in the mempool. */
AVIARY_API uint64_t aviary_node_next_nonce(AviaryNode node, const uint8_t* address);
AVIARY_API uint64_t aviary_node_mempool_size(AviaryNode node);
AVIARY_API AviaryBufferResult aviary_node_state_root(AviaryNode node);
/** Drain collected chain events as a JSON array string (the streaming surface). */
AVIARY_API AviaryBufferResult aviary_node_drain_events(AviaryNode node);
/** Admits an already signed, encoded transaction (for example from
 *  aviary_keystore_sign_transfer). Returns the 32 byte txid. */
AVIARY_API AviaryBufferResult aviary_node_submit_tx(AviaryNode node, const uint8_t* tx, size_t tx_len);
/** Persistence: the whole chain (chain id, genesis credits, every finalized
 *  block with its certificate) as one blob. The mempool is not included. */
AVIARY_API AviaryBufferResult aviary_node_snapshot(AviaryNode node);
/** Rebuilds a node from aviary_node_snapshot. A current snapshot carries the
 *  ledger state and resumes from the tip in constant time; an older one replays
 *  every block through consensus. NULL on a malformed blob. */
AVIARY_API AviaryNode aviary_node_restore(const uint8_t* blob, size_t len);
/** Like aviary_node_restore but always replays the archived blocks through
 *  consensus, verifying every certificate (slow; for audits and tests). */
AVIARY_API AviaryNode aviary_node_restore_replay(const uint8_t* blob, size_t len);
/** The encoded finalized block at [height] (1 based); an error when absent. */
AVIARY_API AviaryBufferResult aviary_node_block_at(AviaryNode node, uint64_t height);
/** Push observer: one JSON object per chain event, delivered synchronously from
 *  inside the node call that produced it. While a hook is set the events are
 *  not buffered for aviary_node_drain_events. A NULL fn removes the hook. */
typedef void (*AviaryEventFn)(void* ctx, const char* json, size_t len);
AVIARY_API void aviary_node_set_event_hook(AviaryNode node, AviaryEventFn fn, void* ctx);
/** Prometheus-format metrics snapshot. */
AVIARY_API AviaryBufferResult aviary_node_metrics(AviaryNode node);
/** State-inclusion proof for [address]; verify with aviary_verify_state_proof. */
AVIARY_API AviaryBufferResult aviary_node_prove_account(
    AviaryNode node, const uint8_t* address);

/* -- Custodial key store (S3) --------------------------------------------------
 * A container that seals account signing secrets at rest. A node never needs to
 * hold a plaintext key: create a store under a passphrase, add accounts, persist
 * the (encrypted) blob, reload it LOCKED, unlock, then reveal a secret only to
 * sign. Each secret is bound to its account address as AAD. */

typedef void* AviaryKeyStore;

/** Create a new key store protected by [passphrase] (returned unlocked). NULL on failure. */
AVIARY_API AviaryKeyStore aviary_keystore_new(const char* passphrase);

/** Seal an account's secret; writes its 32-byte address into [out_address] (>=32 bytes).
 *  Returns 0 on success, nonzero on failure (e.g. store locked). */
AVIARY_API int aviary_keystore_add_account(
    AviaryKeyStore ks, const uint8_t* public_key, size_t pk_len,
    const uint8_t* secret, size_t sk_len, uint8_t* out_address);

/** Reveal the sealed secret for [address] (32 bytes). Requires unlocked. Returns
 *  the secret buffer or an error. */
AVIARY_API AviaryBufferResult aviary_keystore_reveal(
    AviaryKeyStore ks, const uint8_t* address);

/** The stored public key for [address] (32 bytes); an error when unknown. */
AVIARY_API AviaryBufferResult aviary_keystore_public_key(AviaryKeyStore ks, const uint8_t* address);
/** 1 when the store holds [address]. */
AVIARY_API int aviary_keystore_has(AviaryKeyStore ks, const uint8_t* address);
/** Every stored address as a blob list [count u32]([len u32][address 32])*. */
AVIARY_API AviaryBufferResult aviary_keystore_addresses(AviaryKeyStore ks);
/** Forgets the master key; secrets can no longer be revealed or used. */
AVIARY_API void aviary_keystore_lock(AviaryKeyStore ks);
/** Signs a transfer with the sealed secret of [from] WITHOUT revealing it: the
 *  secret is used inside the engine and wiped. The scheme follows the stored
 *  public key (32 bytes Ed25519, longer hybrid). Zero fee gives the version 1
 *  envelope. Returns the encoded transaction; an error when locked or unknown. */
AVIARY_API AviaryBufferResult aviary_keystore_sign_transfer(AviaryKeyStore ks, uint64_t chain_id,
                                                            const uint8_t* from, const uint8_t* to,
                                                            uint64_t amount, uint64_t nonce, uint64_t fee);
/** Signs a transfer from the key store account [from] and admits it, the secret
 *  never leaving the engine. Returns the 32 byte txid. */
AVIARY_API AviaryBufferResult aviary_node_submit_transfer_keystore(AviaryNode node, AviaryKeyStore ks,
                                                                   const uint8_t* from, const uint8_t* to,
                                                                   uint64_t amount, uint64_t nonce, uint64_t fee);
/** Serialise the store (keyring envelope + sealed entries; no plaintext key). */
AVIARY_API AviaryBufferResult aviary_keystore_serialise(AviaryKeyStore ks);

/** Load a serialised store. Comes back LOCKED - call unlock before revealing. NULL on error. */
AVIARY_API AviaryKeyStore aviary_keystore_load(const uint8_t* blob, size_t len);

/** Unlock with a passphrase. Returns 0 on success, nonzero on wrong passphrase. */
AVIARY_API int aviary_keystore_unlock(AviaryKeyStore ks, const char* passphrase);

/** 1 if currently unlocked, else 0. */
AVIARY_API int aviary_keystore_unlocked(AviaryKeyStore ks);

/** Destroy a key-store handle. */
AVIARY_API void aviary_keystore_free(AviaryKeyStore ks);

/* -- Portable account export (tiered encrypted backup) -------------------------
 * Move one signing account off the node under a chosen trust tier, all fail-
 * closed. An account is a (public_key, secret) pair; the "open" calls return it
 * as a self-describing blob: [pub_len:4 BE][public_key][secret] (the same layout
 * aviary_node_new_account_hybrid uses), so a binding unpacks it once and
 * reuses that helper. */

/** A hybrid KEM keypair (X25519 + ML-KEM-768) - a "device key" for the
 *  recipient-bound tiers. Returns [pub_len:4 BE][public][secret], or an error. */
AVIARY_API AviaryBufferResult aviary_hybrid_kem_keygen(void);

/** Tier 1 - passphrase (MolecularVault + Argon2id). ops/mem are Argon2id work
 *  factors; pass 0,0 for the SENSITIVE preset. Returns the sealed envelope. */
AVIARY_API AviaryBufferResult aviary_portable_seal_passphrase(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const char* passphrase, uint64_t ops, size_t mem);
/** Open a passphrase envelope -> [pub_len:4 BE][public][secret]. Error on wrong
 *  passphrase / tamper. */
AVIARY_API AviaryBufferResult aviary_portable_open_passphrase(
    const uint8_t* envelope, size_t env_len, const char* passphrase);

/** Tier 2 - recipient-bound (PQ). Seal to a device's hybrid-KEM public key; only
 *  its KEM secret opens it. No shared secret to phish. */
AVIARY_API AviaryBufferResult aviary_portable_seal_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len);
AVIARY_API AviaryBufferResult aviary_portable_open_to(
    const uint8_t* envelope, size_t env_len, const uint8_t* recipient_kem_secret, size_t rk_len);

/** Tier 3 - signed-recipient (Fortress-grade): recipient-bound AND authenticated.
 *  The receiver verifies the owner's hybrid signature on open. */
AVIARY_API AviaryBufferResult aviary_portable_seal_signed_to(
    const uint8_t* public_key, size_t pk_len, const uint8_t* secret, size_t sk_len,
    const uint8_t* recipient_kem_public, size_t rk_len,
    const uint8_t* sender_sig_secret, size_t ss_len);
AVIARY_API AviaryBufferResult aviary_portable_open_signed_from(
    const uint8_t* envelope, size_t env_len,
    const uint8_t* recipient_kem_secret, size_t rk_len,
    const uint8_t* sender_sig_public, size_t sp_len);

/* -- Threshold accounts (FROST t-of-n; the key never exists) -------------------
 * A t-of-n account whose secret key is never assembled: dealt shares sign in a
 * two-round ceremony, and the aggregate is a STANDARD 64-byte Ed25519 signature
 * - so a threshold-signed transaction verifies through the ordinary Ed25519 path
 * (the chain can't tell). Flow: create() deals n share cards; each participating
 * card runs commit() (keep the nonces, broadcast the commitment); a coordinator
 * opens a ceremony over the fixed roster of commitments; each card signs; the
 * shares aggregate into the group signature.
 *
 * Blob layouts (bindings split these; all multi-byte ints big-endian):
 *   create()   -> [u16 count]  then count x ( [u32 len][share-card bytes] )
 *   commit()   -> [hiding_nonce 32][binding_nonce 32][commitment 66]
 *                where commitment = [id 2][hiding_commit 32][binding_commit 32].
 *                Keep bytes[0..64) (the secret nonces); broadcast bytes[64..130)
 *                (the commitment) to the coordinator.
 *   a roster   = the participating commitments (66 bytes each) concatenated. */

/** Opaque signing-ceremony handle (fixed message + roster). */
typedef void* AviaryCeremony;

/** Deal a t-of-n account: returns the packed share cards (see layout above). */
AVIARY_API AviaryBufferResult aviary_threshold_create(uint16_t n, uint16_t t);

/** A card's identity fields as a 66-byte blob: [id u16 BE][group_pk 32][public
 *  share 32]. The group public key is the account's on-chain author key. */
AVIARY_API AviaryBufferResult aviary_threshold_share_info(
    const uint8_t* card, size_t card_len);

/** Seal a share card for rest (MolecularVault + Argon2id; ops=mem=0 -> SENSITIVE).
 *  The envelope binds the group key + slot, so it can't be relocated. */
AVIARY_API AviaryBufferResult aviary_threshold_share_seal(
    const uint8_t* card, size_t card_len, const char* passphrase, uint64_t ops, size_t mem);
/** Open a sealed share card -> the card bytes. Fails closed on wrong passphrase. */
AVIARY_API AviaryBufferResult aviary_threshold_share_unseal(
    const uint8_t* envelope, size_t env_len, const char* passphrase);

/** Round 1: fresh nonces + commitment for a card (see commit() layout above). */
AVIARY_API AviaryBufferResult aviary_threshold_commit(
    const uint8_t* card, size_t card_len);

/** Open a ceremony over [message] and a [commitments] roster (66 bytes each,
 *  concatenated). Returns a handle, or NULL on malformed input. */
AVIARY_API AviaryCeremony aviary_threshold_ceremony_new(
    const uint8_t* group_pk32, const uint8_t* message, size_t msg_len,
    const uint8_t* commitments, size_t commits_len);
AVIARY_API void aviary_threshold_ceremony_free(AviaryCeremony c);

/** Round 2: this card's 32-byte signature share, using its round-1 [nonces64]. */
AVIARY_API AviaryBufferResult aviary_threshold_sign(
    AviaryCeremony c, const uint8_t* card, size_t card_len, const uint8_t* nonces64);

/** Attribute a share to participant [id] (public share 32 B): 1 valid, 0 invalid. */
AVIARY_API int aviary_threshold_verify_share(
    AviaryCeremony c, uint16_t id, const uint8_t* public32, const uint8_t* sig_share32);

/** Aggregate [count] (id, 32-byte share) pairs -> a 64-byte Ed25519 signature.
 *  The pairs may be in any order; the ceremony reorders them internally. */
AVIARY_API AviaryBufferResult aviary_threshold_aggregate(
    AviaryCeremony c, const uint16_t* ids, const uint8_t* sig_shares, size_t count);

/* -- AssuranceProfile (one security posture, describing every tier) ------------
 * A named posture fixes a consistent tier at every seam. Levels: 1 = standard,
 * 2 = hardened, 3 = fortress. `describe` returns a fixed blob:
 *   [transport u8 (0=mutual, 1=ratcheted)]
 *   [consensus u8 (0=stake-weighted, 1=vrf)]
 *   [threshold_custody u8 (0/1)]
 *   [sealed_mempool  u8 (0/1)]
 *   [sig_algo_count  u8][mandated SigAlgo bytes ...]
 * so a consumer can wire the classes the posture requires. Unknown level -> error. */
AVIARY_API AviaryBufferResult aviary_assurance_describe(uint16_t level);

/* -- Transaction encoding ------------------------------------------------------
 * Encode a transaction from its fields into the canonical wire form (e.g. to
 * seal into the mempool below). [fee] is only carried when version >= 2. */
AVIARY_API AviaryBufferResult aviary_tx_encode(
    uint8_t version, uint64_t chain_id, uint8_t sig_algo, uint64_t fee,
    const uint8_t* body, size_t body_len, const uint8_t* author, size_t author_len,
    const uint8_t* signature, size_t sig_len);

/* -- SealedMempool (anti-MEV: transactions invisible until inclusion) ----------
 * A submitter seals an encoded transaction to the block proposer's hybrid-KEM
 * key; the pool orders by a cleartext fee hint WITHOUT decrypting, and only the
 * proposer opens the winners at build time. A front-runner sees only ciphertext.
 * Submission-only: committed blocks carry ordinary plaintext transactions. */

/** Opaque sealed-mempool handle. */
typedef void* AviarySealedMempool;

/** Seal an encoded transaction to [proposer_kem_public] -> a SealedTx blob (its
 *  cleartext fee hint is the transaction's declared fee). */
AVIARY_API AviaryBufferResult aviary_sealed_tx_seal(
    const uint8_t* tx_encoded, size_t tx_len, const uint8_t* proposer_kem_public, size_t pk_len);

AVIARY_API AviarySealedMempool aviary_sealed_mempool_new(size_t capacity);
/** Admit a SealedTx (no decryption). Returns 1 if admitted, 0 if full/rejected. */
AVIARY_API int aviary_sealed_mempool_add(
    AviarySealedMempool m, const uint8_t* sealed_tx, size_t len);
AVIARY_API size_t aviary_sealed_mempool_size(AviarySealedMempool m);
/** Proposer-only: open up to [max] highest-fee submissions with the KEM secret.
 *  Returns a blob: [count u32] then count x ( [tx_len u32][encoded tx] ). Entries
 *  that don't decrypt / decode, or whose real fee != the committed hint, are
 *  silently skipped. */
AVIARY_API AviaryBufferResult aviary_sealed_mempool_reveal(
    AviarySealedMempool m, const uint8_t* proposer_kem_secret, size_t sk_len, size_t max);
AVIARY_API void aviary_sealed_mempool_free(AviarySealedMempool m);

/* -- CredentialGate (anonymous membership admission - BBS, Option A) -----------
 *  A permissioned chain admits a member to a role by a ZERO-KNOWLEDGE proof that
 *  it holds an issuer-signed credential whose `tier` clears a policy threshold
 *  and whose `not_after` has not passed - revealing nothing else. Option A is
 *  *unlinkable*: two presentations of one credential cannot be correlated (great
 *  privacy, weak Sybil resistance - real per-epoch Sybil resistance is Option B,
 *  blocked on an upstream cryptolib BBS-pseudonym primitive). See
 *  docs/design/CREDENTIAL_GATE.md.
 *
 *  Blob layouts (big-endian):
 *  - BBS issuer keypair: the account blob [pub_len:4][pub 96][sec 32].
 *  - policy: CredentialPolicy::encode() (opaque; produce via _policy_encode).
 *  - credential: [sig_len:4][sig 80]([attr_count:4]([attr_len:4][attr])x) - HOLDER
 *     SECRET (carries member_secret + hidden attributes); store sealed at rest.
 *  - presentation: opaque proof+disclosed blob from _present.
 * --------------------------------------------------------------------------- */

/** A BBS issuer keypair from >=32 bytes of key material. Returns the account blob
 *  [pub_len:4][pub 96][sec 32]. The public key is the chain's issuer identity. */
AVIARY_API AviaryBufferResult aviary_bbs_keygen(
    const uint8_t* key_material, size_t km_len);

/** Canonically encode a membership policy -> the opaque `policy` blob every other
 *  CredentialGate call takes. attribute_count must be 4 (the four well-known
 *  attributes) in this ABI; extra policy-specific attributes are not yet exposed. */
AVIARY_API AviaryBufferResult aviary_credential_policy_encode(
    uint64_t chain_id, const uint8_t* issuer_pk, size_t pk_len,
    const uint8_t* header, size_t header_len, uint32_t attribute_count, uint8_t min_tier);

/** The 32-byte commitment to a policy (hash_tagged) - commit this into genesis so
 *  every node provably agrees on the issuer, schema, and threshold. */
AVIARY_API AviaryBufferResult aviary_credential_policy_hash(
    const uint8_t* policy, size_t policy_len);

/** Issuer side (trusted-issuer model): assemble + BBS-sign a credential. The
 *  32-byte member_secret is generated by the MEMBER and passed to the issuer over
 *  an authenticated channel (blind issuance awaits a cryptolib primitive).
 *  Returns the holder-secret credential blob. */
AVIARY_API AviaryBufferResult aviary_credential_issue(
    const uint8_t* policy, size_t policy_len, const uint8_t* issuer_sk, size_t sk_len,
    const uint8_t* member_secret, size_t ms_len, uint8_t tier, uint64_t not_after,
    uint32_t issuer_epoch);

/** Holder side: build an unlinkable presentation for (epoch, scope, challenge).
 *  Discloses {tier, not_after, issuer_epoch}; proves the rest in zero knowledge. */
AVIARY_API AviaryBufferResult aviary_credential_present(
    const uint8_t* policy, size_t policy_len, const uint8_t* credential, size_t cred_len,
    uint32_t epoch, const char* scope, const uint8_t* challenge, size_t challenge_len);

/** An admission gate (holds the issuer-epoch deny-list). Opaque handle. */
typedef void* AviaryCredentialGate;

/** Open a gate enforcing [policy]. Returns NULL on a malformed policy. */
AVIARY_API AviaryCredentialGate aviary_credential_gate_new(
    const uint8_t* policy, size_t policy_len);
/** Bulk revocation: refuse every credential issued in [epoch]. */
AVIARY_API void aviary_credential_gate_revoke_epoch(
    AviaryCredentialGate g, uint32_t epoch);
/** Verifier side: admit? Runs the full checklist (proof + expiry + revocation +
 *  tier) binding (epoch, scope, challenge, now). Returns 1 admit, 0 reject.
 *  Never fails - any malformed presentation / failed proof is a quiet 0. */
AVIARY_API int aviary_credential_gate_admit(
    AviaryCredentialGate g, uint32_t epoch, const char* scope,
    const uint8_t* challenge, size_t challenge_len, const uint8_t* presentation,
    size_t presentation_len, uint64_t now);
/** Free a gate handle. */
AVIARY_API void aviary_credential_gate_free(AviaryCredentialGate g);

/* -- CredentialGate Option B - pseudonymous (per-epoch Sybil resistance) -------
 *  Real per-epoch Sybil resistance: each presentation carries a deterministic
 *  per-(chain, epoch, scope) pseudonym derived from a holder seed the issuer
 *  never sees (blind BBS issuance). Same member + same epoch + scope -> the SAME
 *  pseudonym; a different epoch -> an unlinkable one. See CREDENTIAL_GATE.md section 3.2.
 *
 *  Blob layouts (each field is a length-prefixed byte string unless noted):
 *  - membership_commit blob:  [commitment][nym_seed 32][secret_prover_blind 32]
 *    - HOLDER SECRET. Decode the FIRST field (commitment) to send to the issuer;
 *     keep the whole blob for _finalize_membership.
 *  - blind_issuance blob:     [sig][entropy 32][tier u8][not_after u64][issuer_epoch u32]
 *    - opaque; the issuer returns it to the holder.
 *  - pseudonymous credential: opaque holder-secret blob (feed to _present_pseudonymous).
 *  - presentation:            opaque proof+nym blob from _present_pseudonymous.
 * --------------------------------------------------------------------------- */

/** Encode a policy with an explicit admission mode (0 = unlinkable / Option A,
 *  1 = pseudonymous / Option B). `aviary_credential_policy_encode` is the
 *  mode-0 shorthand. */
AVIARY_API AviaryBufferResult aviary_credential_policy_encode_ex(
    uint64_t chain_id, const uint8_t* issuer_pk, size_t pk_len, const uint8_t* header,
    size_t header_len, uint32_t attribute_count, uint8_t min_tier, uint8_t mode);

/** Holder step 1: derive a nym seed from [member_secret] and blind-commit it.
 *  Returns the membership-commit blob (holder-secret). */
AVIARY_API AviaryBufferResult aviary_membership_commit(
    const uint8_t* policy, size_t policy_len, const uint8_t* member_secret, size_t ms_len);

/** Issuer step 2: blind-sign the holder's [commitment] (the first field of the
 *  membership-commit blob) with the issuer-set attributes. The issuer never sees
 *  the seed. Returns the blind-issuance blob. */
AVIARY_API AviaryBufferResult aviary_blind_issue(
    const uint8_t* policy, size_t policy_len, const uint8_t* issuer_sk, size_t sk_len,
    const uint8_t* commitment, size_t commitment_len, uint8_t tier, uint64_t not_after,
    uint32_t issuer_epoch);

/** Holder step 3: finalize into a pseudonymous credential (holder-secret). Takes
 *  the whole membership-commit blob + the blind-issuance blob. */
AVIARY_API AviaryBufferResult aviary_finalize_membership(
    const uint8_t* membership_commit, size_t mc_len, const uint8_t* blind_issuance, size_t bi_len);

/** Holder: build a pseudonymous presentation for (epoch, scope, challenge). */
AVIARY_API AviaryBufferResult aviary_credential_present_pseudonymous(
    const uint8_t* policy, size_t policy_len, const uint8_t* credential, size_t cred_len,
    uint32_t epoch, const char* scope, const uint8_t* challenge, size_t challenge_len);

/** Verifier: admit? On success returns the 48-byte pseudonym; on any reject
 *  returns an EMPTY buffer (len 0, no error). Never fails as an error. */
AVIARY_API AviaryBufferResult aviary_credential_gate_admit_pseudonymous(
    AviaryCredentialGate g, uint32_t epoch, const char* scope, const uint8_t* challenge,
    size_t challenge_len, const uint8_t* presentation, size_t presentation_len, uint64_t now);

/** Targeted revocation (section 6.2): refuse this exact pseudonym. */
AVIARY_API void aviary_credential_gate_revoke_nym(
    AviaryCredentialGate g, const uint8_t* nym, size_t nym_len);

/** Sybil cap (section 6.3): record an admitted nym for (epoch, scope). Returns 1 if this
 *  is the first session on it, 0 if a duplicate (the caller rate-limits). */
AVIARY_API int aviary_credential_gate_note_admission(
    AviaryCredentialGate g, uint32_t epoch, const char* scope, const uint8_t* nym,
    size_t nym_len);

/* -- ChainBlueprint + ChainAssembler (composable chains) ----------------------
 *  A chain is a MIXTURE of pluggable components - consensus engine, state
 *  model, proposer policy - each named by a stable id. The mixture is committed
 *  into the genesis hash (it IS chain identity); an id nobody registered, or a
 *  mix the assurance posture forbids, is rejected at assembly (fail-closed).
 *  Custom components are C++ subclasses registered in-process
 *  (ComponentRegistry); every language can DISCOVER what is registered and
 *  DECLARE mixtures naming it.
 *
 *  Blobs: blueprint / genesis / block = canonical encodings (opaque to callers);
 *  id lists = [count u32]([len u32][id])x; keypairs = account blob
 *  [pub_len:4][pub][sec]. All exceptions are isolated at this boundary.
 * --------------------------------------------------------------------------- */

/** A BLS validator keypair from >= 32 bytes of key material -> account blob
 *  [pub 48][sec 32]. */
AVIARY_API AviaryBufferResult aviary_bls_keygen(
    const uint8_t* ikm, size_t ikm_len);

/** Declare a mixture. consensus_id / state_model_id name registered components
 *  ("bft"|"pow"|"pos"|"vrf-pos", "account"|"utxo", or custom); proposer_id may be
 *  "" for the engine default; params are component-interpreted (e.g. a 32-byte
 *  PoW target). Returns the canonical blueprint blob. */
AVIARY_API AviaryBufferResult aviary_blueprint_encode(
    const char* consensus_id, const char* state_model_id, const char* proposer_id,
    const uint8_t* params, size_t params_len);
/** The 32-byte commitment to a mixture. */
AVIARY_API AviaryBufferResult aviary_blueprint_hash(
    const uint8_t* blueprint, size_t blueprint_len);

/** Discovery: the ids a mixture may name. kind: 0 = engines, 1 = state models,
 *  2 = proposer policies. Built-ins plus anything registered in-process. */
AVIARY_API AviaryBufferResult aviary_registry_ids(uint8_t kind);

/** A minimal genesis: chain params + one posture (assurance_level 1/2/3) +
 *  seeded state + validators. state_model: 0 = account (the addr/balance pairs
 *  fund accounts), 1 = utxo (they become genesis outputs). account_addrs are
 *  32 bytes each; validator_pks are 48-byte BLS keys. Returns the genesis blob. */
AVIARY_API AviaryBufferResult aviary_genesis_build(
    uint64_t chain_id, uint64_t timestamp, uint16_t assurance_level, uint8_t state_model,
    const uint8_t* account_addrs, const uint64_t* account_balances, size_t account_count,
    const uint8_t* validator_pks, const uint64_t* validator_powers, size_t validator_count);
/** Commit a mixture into a genesis -> the new genesis blob (its hash changes:
 *  the mixture is chain identity). */
AVIARY_API AviaryBufferResult aviary_genesis_commit_blueprint(
    const uint8_t* genesis, size_t genesis_len, const uint8_t* blueprint, size_t blueprint_len);
/** The 32-byte genesis hash (chain identity). */
AVIARY_API AviaryBufferResult aviary_genesis_hash(
    const uint8_t* genesis, size_t genesis_len);

/** An assembled chain: the engine the mixture named, behind the uniform
 *  propose -> (seal | mine) -> submit flow. Opaque handle. */
typedef void* AviaryChain;

/** Assemble a node for validator [me] from [genesis] (using its committed
 *  mixture, or the posture-driven default). Returns NULL on ANY reject: an
 *  unknown component id, a state id that does not match the seeding, or an
 *  engine the posture forbids (fail-closed). VRF engines need extra material;
 *  use the C++ API for those. */
AVIARY_API AviaryChain aviary_chain_assemble(
    const uint8_t* genesis, size_t genesis_len, uint32_t me,
    const uint8_t* bls_pub, size_t pub_len, const uint8_t* bls_sec, size_t sec_len);
AVIARY_API uint64_t aviary_chain_height(AviaryChain c);
/** The mixture this chain runs (encoded blueprint). */
AVIARY_API AviaryBufferResult aviary_chain_blueprint(AviaryChain c);
/** Propose the next (empty) block -> encoded Block. Errors if this node may not
 *  propose now (e.g. not the leader). */
AVIARY_API AviaryBufferResult aviary_chain_propose(
    AviaryChain c, uint64_t timestamp);
/** PoS / VRF-PoS: the leader's justification for [block]. Returns an EMPTY
 *  buffer for engines that do not seal (PoW, BFT, custom). */
AVIARY_API AviaryBufferResult aviary_chain_seal(
    AviaryChain c, const uint8_t* block, size_t block_len);
/** PoW: search a solving nonce -> the solved, encoded Block. Errors if the
 *  engine is not PoW or no solution was found within the bound. */
AVIARY_API AviaryBufferResult aviary_chain_mine(
    AviaryChain c, const uint8_t* block, size_t block_len);
/** Verify [justification] + apply [block] atomically. 1 committed, 0 rejected. */
AVIARY_API int aviary_chain_submit(
    AviaryChain c, const uint8_t* block, size_t block_len,
    const uint8_t* justification, size_t justification_len);
AVIARY_API void aviary_chain_free(AviaryChain c);

/* -- Subchains: a parent block anchors a child chain's finalized headers -----
 *  (docs/design/SUBCHAINS.md). A child is registered at the parent's genesis
 *  by its genesis HEADER + fixed validator set; a parent block carries anchors
 *  (consecutive certified child headers) which every parent validator verifies
 *  with the light-client rule; the child's tip is committed in the parent
 *  header's subchain_root. Cross-chain proofs descend hop by hop from a parent
 *  header to a state entry on a descendant. v1 anchors BFT children only.
 *  Blob LISTS below are [count u32]([len u32][bytes])x (big-endian). */

/** A genesis's genesis HEADER (the identity a parent registers a child by). */
AVIARY_API AviaryBufferResult aviary_genesis_header(
    const uint8_t* genesis, size_t genesis_len);
/** Encode a child registration: its chain id, genesis header, validators
 *  (48-byte BLS keys + powers) and its own subtree depth (0 = leaf). */
AVIARY_API AviaryBufferResult aviary_child_registration_encode(
    uint64_t chain_id, const uint8_t* genesis_header, size_t header_len,
    const uint8_t* validator_pks, const uint64_t* validator_powers, size_t validator_count,
    uint8_t child_max_depth);
/** Encode a parent's subchain policy from a LIST of registrations (ascending,
 *  unique chain ids). 0 for an unlimited per-block / per-anchor bound. */
AVIARY_API AviaryBufferResult aviary_subchain_policy_encode(
    uint16_t max_children, uint8_t max_depth, uint16_t max_anchors_per_block,
    uint16_t max_headers_per_anchor, const uint8_t* registrations, size_t registrations_len);
/** Commit a subchain policy into a genesis -> the parent's genesis blob (hash changes). */
AVIARY_API AviaryBufferResult aviary_genesis_commit_subchain_policy(
    const uint8_t* genesis, size_t genesis_len, const uint8_t* policy, size_t policy_len);
/** Bind a child genesis to its parent (id + 32-byte parent genesis hash) ->
 *  the child's genesis blob (hash changes: it can never be re-parented). */
AVIARY_API AviaryBufferResult aviary_genesis_commit_parent_binding(
    const uint8_t* genesis, size_t genesis_len, uint64_t parent_chain_id,
    const uint8_t* parent_genesis_hash);
/** The encoded header of an encoded block. */
AVIARY_API AviaryBufferResult aviary_block_header(
    const uint8_t* block, size_t block_len);
/** A header's 32-byte subchain_root (all-zero below version 3). */
AVIARY_API AviaryBufferResult aviary_header_subchain_root(
    const uint8_t* header, size_t header_len);
/** A child header + the certificate that finalized it -> CertifiedHeader bytes. */
AVIARY_API AviaryBufferResult aviary_certified_header_encode(
    const uint8_t* header, size_t header_len, const uint8_t* certificate, size_t certificate_len);
/** An anchor: one child's CONSECUTIVE certified headers (a LIST) -> Anchor bytes. */
AVIARY_API AviaryBufferResult aviary_anchor_encode(
    uint64_t child_chain_id, const uint8_t* certified_headers, size_t certified_headers_len);
/** Propose the next block on a PARENT chain, anchoring a LIST of anchors
 *  (ascending child id). Errors if any anchor is invalid - never proposes it. */
AVIARY_API AviaryBufferResult aviary_chain_propose_anchored(
    AviaryChain c, uint64_t timestamp, const uint8_t* anchors, size_t anchors_len);
/** BFT engines: this validator's precommit aggregated into a commit
 *  certificate for [block] -> CommitCertificate bytes (the justification for
 *  aviary_chain_submit, and what a parent anchors). Errors if the engine
 *  is not BFT or this validator alone does not reach quorum (gather the other
 *  validators' precommits with the C++ API). */
AVIARY_API AviaryBufferResult aviary_chain_certify(
    AviaryChain c, const uint8_t* block, size_t block_len);
/* -- Light client: follow a header chain with certificates only --------------
 *  Sequential accept, plus the OVERLAP (skipping) rule: jump to a far header
 *  when its own set certifies it AND the signers hold more than trust_level
 *  (default 1/3) of the set the client already trusts. Validator sets are
 *  the canonical ValidatorSet encoding: [count u32]([len u32][pk][power u64])x
 *  (what aviary_chain_validators returns). Handles are not thread-safe. */
typedef void* AviaryLightClient;
/** Trust a header (e.g. from aviary_genesis_header) and the set in force
 *  for the next header. NULL on malformed input. */
AVIARY_API AviaryLightClient aviary_light_client_new(
    uint64_t chain_id, const uint8_t* validators, size_t validators_len,
    const uint8_t* trusted_header, size_t header_len);
/** Sequential rule: 1 accepted (tip advanced), 0 refused. */
AVIARY_API int aviary_light_client_accept(
    AviaryLightClient lc, const uint8_t* header, size_t header_len,
    const uint8_t* certificate, size_t certificate_len);
/** Skipping rule with [validators] = the set in force at the header's height.
 *  1 accepted, 0 refused (hard: malformed / mis-certified / not ahead / over
 *  max_skip), 2 insufficient overlap (bisect: try an intermediate header). */
AVIARY_API int aviary_light_client_accept_skipping(
    AviaryLightClient lc, const uint8_t* header, size_t header_len,
    const uint8_t* certificate, size_t certificate_len,
    const uint8_t* validators, size_t validators_len);
AVIARY_API uint64_t aviary_light_client_height(AviaryLightClient lc);
/** The trusted tip header. */
AVIARY_API AviaryBufferResult aviary_light_client_tip(AviaryLightClient lc);
/** The set in force for the next header (follows rotations). */
AVIARY_API AviaryBufferResult aviary_light_client_validators(AviaryLightClient lc);
/** Overlap threshold numerator/denominator (default 1/3; bad values ignored). */
AVIARY_API void aviary_light_client_set_trust_level(
    AviaryLightClient lc, uint32_t numerator, uint32_t denominator);
/** Largest single jump in blocks (0 = unbounded). Tie to the unbonding period. */
AVIARY_API void aviary_light_client_set_max_skip(AviaryLightClient lc, uint64_t blocks);
AVIARY_API void aviary_light_client_free(AviaryLightClient lc);

/* -- Validator-set changes (coordinated rotation, BFT engines) ----------------
 *  Operators agree a new set out of band and each SCHEDULES it; the next block
 *  this node proposes announces it; aviary_chain_certify refuses to vote
 *  for an unscheduled change; once a rotation block is certified by >2/3 of
 *  the current set it takes effect for the next block (this node follows it
 *  even if it did not schedule it, and stops certifying if it left the set). */

/** Schedule the next validator set (48-byte BLS keys + powers). Returns 1, or
 *  0 if the engine is not BFT or the set is malformed. */
AVIARY_API int aviary_chain_schedule_validators(
    AviaryChain c, const uint8_t* validator_pks, const uint64_t* validator_powers,
    size_t validator_count);
/** The validator set in force for the next block: [count u32]([len u32][pk][power u64])x. */
AVIARY_API AviaryBufferResult aviary_chain_validators(AviaryChain c);

/** A parent's last anchored header for [child_chain_id] (errors if this chain
 *  is not a parent or the child is not registered). */
AVIARY_API AviaryBufferResult aviary_chain_child_tip(
    AviaryChain c, uint64_t child_chain_id);
/** A proof hop from this parent's CURRENT registry into [child_chain_id]. */
AVIARY_API AviaryBufferResult aviary_chain_subchain_hop(
    AviaryChain c, uint64_t child_chain_id);
/** State-inclusion proof for a 32-byte address on this (account-model) chain. */
AVIARY_API AviaryBufferResult aviary_chain_prove_account(
    AviaryChain c, const uint8_t* address);
/** A cross-chain proof: a LIST of hops (parent -> ... -> the chain holding the
 *  entry) + that chain's state proof -> SubchainProof bytes. */
AVIARY_API AviaryBufferResult aviary_subchain_proof_encode(
    const uint8_t* hops, size_t hops_len, const uint8_t* state_proof, size_t state_proof_len);
/** 1 iff [proof] is committed under the 32-byte [parent_subchain_root] all the
 *  way down and its state proof holds; 0 otherwise. Never throws. */
AVIARY_API int aviary_subchain_proof_verify(
    const uint8_t* parent_subchain_root, const uint8_t* proof, size_t proof_len);

/* -- Chain tree: declare a FAMILY of chains and build / drive it together ----
 *  A recursive spec - each node a chain with its own mixture, accounts,
 *  validators and children (subchains). build() resolves genesis bottom-up,
 *  bindings top-down and assembles every node (NULL on any reject - see
 *  aviary_chain_tree_spec_error for the reason). produce() proposes
 *  with the node's pending child anchors, justifies in-process (BFT: the
 *  locally held validator secrets must reach quorum; PoS/VRF seal; PoW mine),
 *  submits and archives - blocks flow upward automatically. prove_account
 *  yields a SubchainProof from the ROOT header. Hooks let ANY language extend
 *  the tree (transactions to include; finalization observer). Not thread-safe. */
typedef void* AviaryChainTree;

/** Encode one spec node. children = a LIST of encoded child nodes (encode
 *  bottom-up). validator_secrets: 32 B each, or NULL / all-zero for validators
 *  this process does not hold. blueprint = aviary_blueprint_encode output.
 *  balances are u64 per account. 0 for unlimited anchor bounds. */
AVIARY_API AviaryBufferResult aviary_chain_tree_node_encode(
    uint64_t chain_id, const uint8_t* blueprint, size_t blueprint_len, uint16_t assurance_level,
    const uint8_t* account_addrs, const uint64_t* account_balances, size_t account_count,
    const uint8_t* validator_pks, const uint8_t* validator_secrets, const uint64_t* validator_powers,
    size_t validator_count, uint16_t max_anchors_per_block, uint16_t max_headers_per_anchor,
    const uint8_t* children, size_t children_len);
/** Why a spec is invalid (empty buffer = valid). Structural only; a mixture the
 *  registry / posture rejects is only discovered by build. */
AVIARY_API AviaryBufferResult aviary_chain_tree_spec_error(
    const uint8_t* spec, size_t spec_len);
/** Build the whole family from an encoded ROOT node. NULL on any reject. */
AVIARY_API AviaryChainTree aviary_chain_tree_build(const uint8_t* spec, size_t spec_len);
/** Chain ids in pre-order (parents first): [count u32][u64]x. */
AVIARY_API AviaryBufferResult aviary_chain_tree_chain_ids(AviaryChainTree t);
AVIARY_API uint64_t aviary_chain_tree_height(AviaryChainTree t, uint64_t chain_id);
/** The node's final genesis blob (policy + binding committed). */
AVIARY_API AviaryBufferResult aviary_chain_tree_genesis(AviaryChainTree t, uint64_t chain_id);
/** The node's current tip header. */
AVIARY_API AviaryBufferResult aviary_chain_tree_tip(AviaryChainTree t, uint64_t chain_id);
/** How many anchors produce() would include on [chain_id] right now. */
AVIARY_API uint32_t aviary_chain_tree_pending_anchors(AviaryChainTree t, uint64_t chain_id);
/** Produce + finalize the next block on [chain_id] -> the encoded Block. Errors with the reason. */
AVIARY_API AviaryBufferResult aviary_chain_tree_produce(
    AviaryChainTree t, uint64_t chain_id, uint64_t timestamp);
/** Cross-chain proof of a 32-byte address on [chain_id] from the ROOT header (no hops for the root). */
AVIARY_API AviaryBufferResult aviary_chain_tree_prove_account(
    AviaryChainTree t, uint64_t chain_id, const uint8_t* address);
/** 1 iff [proof] is committed by the root's CURRENT header. */
AVIARY_API int aviary_chain_tree_verify(AviaryChainTree t, const uint8_t* proof, size_t proof_len);
/** Transactions to include when [chain_id] produces at [timestamp]: write a
 *  LIST of encoded transactions into [out] (<= out_cap), set *out_len; return
 *  1, or 0 to include none. Runs synchronously inside produce(). */
typedef int (*AviaryTxSourceFn)(void* ctx, uint64_t chain_id, uint64_t timestamp,
                                      uint8_t* out, size_t out_cap, size_t* out_len);
/** Events and metrics of an assembled chain: block_finalized / block_rejected
 *  per submit, as the node's JSON event stream, plus Prometheus text. */
AVIARY_API AviaryBufferResult aviary_chain_drain_events(AviaryChain c);
AVIARY_API void aviary_chain_set_event_hook(AviaryChain c, AviaryEventFn fn, void* ctx);
AVIARY_API AviaryBufferResult aviary_chain_metrics(AviaryChain c);

/* -- Record model: signed application objects -----------------------------------
 * A chain whose mixture names the "record" state model stores application
 * objects instead of balances: a 32 byte key chosen by the application, the
 * owner (the address of the author that created it), a version that grows by
 * one per write, and an opaque value of up to 64 KiB. Only the owner may update
 * (op 0, value = new content), transfer (op 1, value = new 32 byte owner
 * address) or delete (op 2, empty value) a record; replay protection is a per
 * author nonce. Build the genesis with state_model 2 and a blueprint naming
 * "record", assemble it, add signed transactions, propose and submit. */

/** The chain id this assembled chain runs. */
AVIARY_API uint64_t aviary_chain_chain_id(AviaryChain c);
/** The committed 32 byte state root of this assembled chain. */
AVIARY_API AviaryBufferResult aviary_chain_state_root(AviaryChain c);
/** The 32 byte chain address of a public key (any signature scheme). */
AVIARY_API AviaryBufferResult aviary_address_of(const uint8_t* public_key, size_t public_key_len);
/** Canonical record transaction body: u64 nonce || key || u8 op || bytes value. */
AVIARY_API AviaryBufferResult aviary_record_write_encode(uint64_t nonce, const uint8_t* key, uint8_t op,
                                                         const uint8_t* value, size_t value_len);
/** A signed, encoded record transaction from a 64 byte Ed25519 secret
 *  (seed || public, as every other Ed25519 secret in this ABI). */
AVIARY_API AviaryBufferResult aviary_record_tx_sign(uint64_t chain_id, uint64_t nonce, const uint8_t* key,
                                                    uint8_t op, const uint8_t* value, size_t value_len,
                                                    const uint8_t* secret, size_t secret_len);
/** Queues an encoded, signed transaction for this chain's next proposal after
 *  checking it against the chain id, the accepted signature schemes and the
 *  state as it will be once the earlier queued transactions apply. Returns the
 *  32 byte txid, or an error naming the reason. Works for every state model. */
AVIARY_API AviaryBufferResult aviary_chain_add_tx(AviaryChain c, const uint8_t* tx, size_t tx_len);
/** Transactions queued for the next proposal. */
AVIARY_API uint64_t aviary_chain_pending_count(AviaryChain c);
/** The committed record at [key]: owner(32) || u64 version || bytes value.
 *  An error when the chain is not a record chain or no such record exists. */
AVIARY_API AviaryBufferResult aviary_chain_record(AviaryChain c, const uint8_t* key);
/** The next nonce [address] must use on a record chain. */
AVIARY_API uint64_t aviary_chain_record_nonce(AviaryChain c, const uint8_t* address);
/** A state inclusion proof for the record at [key] (verify with
 *  aviary_verify_state_proof against the chain's state root). */
AVIARY_API AviaryBufferResult aviary_chain_prove_record(AviaryChain c, const uint8_t* key);
/** Reads the record a state proof commits to: key(32) || owner(32) || u64
 *  version || bytes value. Verify the proof first. */
AVIARY_API AviaryBufferResult aviary_record_from_proof(const uint8_t* proof, size_t proof_len);
/** A record write signed by the key store account at [from] (Ed25519 or
 *  hybrid, following the stored key); the secret never leaves the engine.
 *  Errors when the store is locked or [from] is unknown. */
AVIARY_API AviaryBufferResult aviary_keystore_sign_record(AviaryKeyStore ks, uint64_t chain_id,
                                                          const uint8_t* from, uint64_t nonce,
                                                          const uint8_t* key, uint8_t op,
                                                          const uint8_t* value, size_t value_len);
/** A record write signed by a hybrid Ed25519 + ML-DSA-65 account (version 2
 *  envelope); [public_key] and [secret] are the hybrid blobs. */
AVIARY_API AviaryBufferResult aviary_record_tx_sign_hybrid(uint64_t chain_id, uint64_t nonce,
                                                           const uint8_t* key, uint8_t op,
                                                           const uint8_t* value, size_t value_len,
                                                           const uint8_t* public_key, size_t public_len,
                                                           const uint8_t* secret, size_t secret_len);

/** Observer: a block was finalized on [chain_id]. */
typedef void (*AviaryFinalizedFn)(void* ctx, uint64_t chain_id, const uint8_t* block, size_t block_len);
#define AVIARY_MAX_TX_LIST (1u << 20)
AVIARY_API void aviary_chain_tree_set_tx_source(AviaryChainTree t, AviaryTxSourceFn fn, void* ctx);
AVIARY_API void aviary_chain_tree_set_finalized_hook(AviaryChainTree t, AviaryFinalizedFn fn, void* ctx);
AVIARY_API void aviary_chain_tree_free(AviaryChainTree t);

/* -- Networking: consensus over gossip without a socket or a clock ----------
 *  A NetNode is a validator (or an observer) that runs BFT consensus over a
 *  gossip mesh but owns no socket and no timer. Every input returns an
 *  OUTBOUND blob telling the caller what to do:
 *    [relays u32]([peer u32][len u32][gossip bytes])*
 *    [timeouts u32]([step u8][round u32])*
 *  The caller sends each relay to that peer over its own transport, arms each
 *  timeout with a duration of its choosing, and feeds what arrives back in with
 *  aviary_net_node_on_gossip / aviary_net_node_on_timeout. Steps: 0 propose,
 *  1 prevote, 2 precommit. [me] is this node's validator index, or
 *  AVIARY_NET_OBSERVER to follow the chain without voting (bls keys may then
 *  be empty). Blocks carry the transactions set with
 *  aviary_net_node_set_transactions when this node is the proposer. */
typedef void* AviaryNetNode;
#define AVIARY_NET_OBSERVER 0xFFFFFFFFu
AVIARY_API AviaryNetNode aviary_net_node_new(const uint8_t* genesis, size_t genesis_len, uint32_t me,
                                             const uint8_t* bls_public, size_t bls_public_len,
                                             const uint8_t* bls_secret, size_t bls_secret_len,
                                             uint64_t timestamp);
AVIARY_API void aviary_net_node_add_peer(AviaryNetNode n, uint32_t peer);
/** Encoded transactions as a blob list [count u32]([len u32][tx])*. 1 ok, 0 malformed.
 *  Cleared when the height commits: set the next height's list after the
 *  commit and before aviary_net_node_advance, which proposes it. */
AVIARY_API int aviary_net_node_set_transactions(AviaryNetNode n, const uint8_t* txs, size_t txs_len);
AVIARY_API AviaryBufferResult aviary_net_node_start(AviaryNetNode n);
AVIARY_API AviaryBufferResult aviary_net_node_on_gossip(AviaryNetNode n, uint32_t from,
                                                        const uint8_t* payload, size_t payload_len);
AVIARY_API AviaryBufferResult aviary_net_node_on_timeout(AviaryNetNode n, uint8_t step, uint32_t round);
/** Precondition: committed. Rolls into the next height; an error otherwise. */
AVIARY_API AviaryBufferResult aviary_net_node_advance(AviaryNetNode n);
AVIARY_API int aviary_net_node_committed(AviaryNetNode n);
AVIARY_API uint64_t aviary_net_node_height(AviaryNetNode n);
AVIARY_API int aviary_net_node_peer_banned(AviaryNetNode n, uint32_t peer);
/** Every unique consensus payload of the current height, as a blob list; replay
 *  it to a peer whose link (re)connects after consensus started. */
AVIARY_API AviaryBufferResult aviary_net_node_replay_log(AviaryNetNode n);
AVIARY_API AviaryBufferResult aviary_net_node_tip(AviaryNetNode n);
AVIARY_API AviaryBufferResult aviary_net_node_state_root(AviaryNetNode n);
AVIARY_API uint64_t aviary_net_node_balance(AviaryNetNode n, const uint8_t* address);
AVIARY_API AviaryBufferResult aviary_net_node_prove_account(AviaryNetNode n, const uint8_t* address);
/** Record model networks: the committed record at [key] as owner(32) || u64
 *  version || bytes value; an error when the network is not a record chain
 *  or no such record exists. */
AVIARY_API AviaryBufferResult aviary_net_node_record(AviaryNetNode n, const uint8_t* key);
/** Record model networks: the next nonce [address] must use. */
AVIARY_API uint64_t aviary_net_node_record_nonce(AviaryNetNode n, const uint8_t* address);
/** Record model networks: a state inclusion proof for the record at [key]. */
AVIARY_API AviaryBufferResult aviary_net_node_prove_record(AviaryNetNode n, const uint8_t* key);
/** Events (block_finalized per committed height) and metrics (chain height,
 *  blocks finalized, gossip received / relayed / dropped, timeouts, peers
 *  banned) of a networked node, in the node's JSON and Prometheus formats. */
AVIARY_API AviaryBufferResult aviary_net_node_drain_events(AviaryNetNode n);
AVIARY_API void aviary_net_node_set_event_hook(AviaryNetNode n, AviaryEventFn fn, void* ctx);
AVIARY_API AviaryBufferResult aviary_net_node_metrics(AviaryNetNode n);
AVIARY_API void aviary_net_node_free(AviaryNetNode n);

/* -- Secure session: the mutually authenticated, post-quantum hybrid link ----
 *  Wrap each gossip relay in a SecureSession between the two peers. The
 *  initiator needs the responder's KEM public key and its own signature key
 *  pair; the responder needs its full identity. Handshake: the initiator's
 *  first message goes to the responder, whose first message comes back and
 *  completes the initiator. Both sides then encrypt and decrypt frames and
 *  MUST pin peer_identity against their allow list. */
typedef void* AviarySecureSession;
/** A fresh identity as a blob list: [kem public][kem secret][sig public][sig secret]. */
AVIARY_API AviaryBufferResult aviary_secure_identity_generate(void);
AVIARY_API AviarySecureSession aviary_secure_initiate(const uint8_t* responder_kem_public, size_t rk_len,
                                                      const uint8_t* sig_public, size_t sp_len,
                                                      const uint8_t* sig_secret, size_t ss_len);
AVIARY_API AviarySecureSession aviary_secure_accept(const uint8_t* kem_public, size_t kp_len,
                                                    const uint8_t* kem_secret, size_t ks_len,
                                                    const uint8_t* sig_public, size_t sp_len,
                                                    const uint8_t* sig_secret, size_t ss_len,
                                                    const uint8_t* first_message, size_t fm_len);
/** The handshake message this side must send: msg1 for an initiator, msg2 for a responder. */
AVIARY_API AviaryBufferResult aviary_secure_handshake_message(AviarySecureSession s);
/** Initiator: consume the responder's message. 1 open, 0 refused. */
AVIARY_API int aviary_secure_complete(AviarySecureSession s, const uint8_t* second_message, size_t sm_len);
AVIARY_API int aviary_secure_open(AviarySecureSession s);
/** The authenticated remote signing public key; empty until open. */
AVIARY_API AviaryBufferResult aviary_secure_peer_identity(AviarySecureSession s);
AVIARY_API AviaryBufferResult aviary_secure_encrypt(AviarySecureSession s, const uint8_t* plaintext, size_t len);
AVIARY_API AviaryBufferResult aviary_secure_decrypt(AviarySecureSession s, const uint8_t* frame, size_t len);
AVIARY_API void aviary_secure_free(AviarySecureSession s);

/* -- Address book: self signed peer records and peer exchange -----------------
 * A peer record says where to reach a validator (host, port) and which keys
 * authenticate it (hybrid KEM, hybrid signature, optional ratchet prekey); the
 * owner signs it. An address book is the set of records a node knows. add is
 * the trusted local path (no verification); merge is the peer exchange
 * boundary: it verifies every incoming record and never overwrites a known id.
 * Records and books use the deterministic codec, so two nodes that learned the
 * same peers hold byte identical books. */

/** A record signed with the owner's hybrid signature secret (which must match
 *  [sig_pk]); returns its canonical bytes. [prekey] may be empty. */
AVIARY_API AviaryBufferResult aviary_peer_record_sign(uint32_t id, const char* host, uint16_t port,
                                                      const uint8_t* kem_pk, size_t kem_len,
                                                      const uint8_t* sig_pk, size_t sig_len,
                                                      const uint8_t* prekey, size_t prekey_len,
                                                      const uint8_t* sig_secret, size_t secret_len);
/** 1 when the record decodes and its owner signature verifies, else 0. */
AVIARY_API int aviary_peer_record_verify(const uint8_t* record, size_t len);
/** The record's fields as a blob list: id (u32 BE), host, port (u16 BE),
 *  kem_pk, sig_pk, prekey, signature. */
AVIARY_API AviaryBufferResult aviary_peer_record_fields(const uint8_t* record, size_t len);

/** Opaque address book handle. */
typedef void* AviaryAddressBook;
AVIARY_API AviaryAddressBook aviary_address_book_new(void);
/** Decodes a canonical book (does not verify; merge it into a book to verify).
 *  NULL on malformed or non canonical input. */
AVIARY_API AviaryAddressBook aviary_address_book_decode(const uint8_t* book, size_t len);
AVIARY_API AviaryBufferResult aviary_address_book_encode(AviaryAddressBook b);
/** Trusted insert of an encoded record (yours, or a configured seed). */
AVIARY_API int aviary_address_book_add(AviaryAddressBook b, const uint8_t* record, size_t len);
/** Learns the verified records of an encoded book that are not yet known.
 *  Returns how many were added, or -1 when the book does not decode. */
AVIARY_API int64_t aviary_address_book_merge(AviaryAddressBook b, const uint8_t* book, size_t len);
/** The encoded record for [id]; an error when the id is unknown. */
AVIARY_API AviaryBufferResult aviary_address_book_get(AviaryAddressBook b, uint32_t id);
AVIARY_API int aviary_address_book_contains(AviaryAddressBook b, uint32_t id);
AVIARY_API uint64_t aviary_address_book_size(AviaryAddressBook b);
/** Every record in ascending id order, as a blob list of encoded records. */
AVIARY_API AviaryBufferResult aviary_address_book_records(AviaryAddressBook b);
AVIARY_API void aviary_address_book_free(AviaryAddressBook b);

/* -- Noise session: Noise_XX_25519_ChaChaPoly_SHA256 with a PQ identity proof --
 * The standards based link for peers that speak Noise. Three handshake
 * messages (initiator msg0, responder msg1, initiator msg2), then each side
 * sends its identity proof, which binds its hybrid signature key to this
 * handshake hash. encrypt and decrypt refuse until the peer's proof verified.
 * Authentication is post quantum; confidentiality is classical (X25519); use
 * SecureSession when post quantum confidentiality is required. Always pin
 * aviary_noise_peer_identity against your allow list. */

/** A fresh identity as a blob list: noise static public, noise static
 *  secret (X25519), hybrid signature public, hybrid signature secret. */
AVIARY_API AviaryBufferResult aviary_noise_identity_generate(void);
typedef void* AviaryNoiseSession;
/** Initiator: begins the handshake; aviary_noise_handshake_message holds msg0. */
AVIARY_API AviaryNoiseSession aviary_noise_initiate(const uint8_t* identity, size_t identity_len,
                                                    const uint8_t* prologue, size_t prologue_len);
/** Responder: consumes msg0; aviary_noise_handshake_message holds msg1. NULL on
 *  a malformed message or identity. */
AVIARY_API AviaryNoiseSession aviary_noise_accept(const uint8_t* identity, size_t identity_len,
                                                  const uint8_t* msg0, size_t msg0_len,
                                                  const uint8_t* prologue, size_t prologue_len);
/** The handshake message this side produced when it was created. */
AVIARY_API AviaryBufferResult aviary_noise_handshake_message(AviaryNoiseSession s);
/** Initiator: consumes msg1 and returns msg2. */
AVIARY_API AviaryBufferResult aviary_noise_complete_initiator(AviaryNoiseSession s, const uint8_t* msg1, size_t len);
/** Responder: consumes msg2. */
AVIARY_API int aviary_noise_complete_responder(AviaryNoiseSession s, const uint8_t* msg2, size_t len);
/** This side's identity proof, sent once after the handshake. */
AVIARY_API AviaryBufferResult aviary_noise_identity_proof(AviaryNoiseSession s);
/** Verifies the peer's identity proof; AVIARY_OK on success. */
AVIARY_API int aviary_noise_verify_identity_proof(AviaryNoiseSession s, const uint8_t* proof, size_t len);
/** 1 once the handshake is done and the peer's proof verified. */
AVIARY_API int aviary_noise_open(AviaryNoiseSession s);
/** The peer's authenticated hybrid signature key. */
AVIARY_API AviaryBufferResult aviary_noise_peer_identity(AviaryNoiseSession s);
/** The peer's X25519 static key learned in the handshake. */
AVIARY_API AviaryBufferResult aviary_noise_peer_static(AviaryNoiseSession s);
AVIARY_API AviaryBufferResult aviary_noise_handshake_hash(AviaryNoiseSession s);
AVIARY_API AviaryBufferResult aviary_noise_encrypt(AviaryNoiseSession s, const uint8_t* plaintext, size_t len);
AVIARY_API AviaryBufferResult aviary_noise_decrypt(AviaryNoiseSession s, const uint8_t* record, size_t len);
AVIARY_API void aviary_noise_free(AviaryNoiseSession s);

/* -- Admission handshake: anonymous membership before a peer joins the mesh ---
 * The verifier sends a fresh challenge, the peer answers with an unlinkable
 * credential presentation for the "gossip" scope, the verifier admits or
 * refuses. Transport agnostic: move the bytes over your own socket. */

/** A fresh challenge for [epoch]: nonce (32 bytes, keep it) || wire message. */
AVIARY_API AviaryBufferResult aviary_admission_challenge(uint32_t epoch);
/** The peer's answer to an encoded challenge, from its credential under the
 *  encoded [policy]. */
AVIARY_API AviaryBufferResult aviary_admission_answer(const uint8_t* policy, size_t policy_len,
                                                      const uint8_t* credential, size_t credential_len,
                                                      const uint8_t* challenge, size_t challenge_len);
/** 1 when [presentation] admits under [gate] for [epoch], the verifier's
 *  [nonce] and [now]; 0 otherwise. Never fails loudly. */
AVIARY_API int aviary_admission_granted(AviaryCredentialGate gate, uint32_t epoch,
                                        const uint8_t* nonce, size_t nonce_len,
                                        const uint8_t* presentation, size_t presentation_len,
                                        uint64_t now);

/* -- Foreign components: implement engines / proposers in ANY language --------
 *  The consensus pipeline stays in C++ (a foreign engine proposes and applies
 *  via the Ledger); the foreign code supplies the JUSTIFICATION policy through
 *  C callbacks. Callbacks are invoked synchronously inside consensus and MUST
 *  be deterministic (no clock / randomness / map order) and must not re-enter
 *  the chain. [ctx] is passed back verbatim (an opaque handle to your object).
 *  A foreign engine seals via aviary_chain_seal (-> your justify) and
 *  submits via aviary_chain_submit (-> your verify, then apply). */

/** Justify a block this node proposed: write up to out_cap bytes into [out],
 *  set *out_len. Return 1 on success, 0 to refuse. */
typedef int (*AviaryJustifyFn)(void* ctx, const uint8_t* block, size_t block_len,
                                     uint8_t* out, size_t out_cap, size_t* out_len);
/** Verify a justification for a block. Return 1 valid, 0 invalid. */
typedef int (*AviaryVerifyFn)(void* ctx, const uint8_t* block, size_t block_len,
                                    const uint8_t* justification, size_t just_len);
/** Leader election: the proposer index for [height] among [validator_count]. */
typedef uint32_t (*AviaryProposerFn)(void* ctx, uint64_t height, uint32_t validator_count);

/** Register a foreign engine under [id] (chain identity - never rename).
 *  Returns 1 on success, 0 on bad arguments. */
AVIARY_API int aviary_registry_add_engine(
    const char* id, AviaryJustifyFn justify, AviaryVerifyFn verify, void* ctx,
    int grind_proof);
/** Register a foreign proposer policy under [id]. Returns 1 on success. */
AVIARY_API int aviary_registry_add_proposer(
    const char* id, AviaryProposerFn fn, void* ctx);
/** Max justification size a foreign justify callback may produce. */
#define AVIARY_MAX_JUSTIFICATION 4096


/* -- Interop address derivation (showcase) ----------------------------------- */

/** Ethereum EIP-55 checksum address (string) from a 32-byte secp256k1 secret. */
AVIARY_API AviaryBufferResult aviary_eth_address(const uint8_t* secret32);
/** Bitcoin P2WPKH (bech32) address (string) from a 32-byte secp256k1 secret. */
AVIARY_API AviaryBufferResult aviary_btc_address(const uint8_t* secret32);
/** Solana address (base58 string) from a 64-byte Ed25519 secret. */
AVIARY_API AviaryBufferResult aviary_sol_address(const uint8_t* ed_secret64);

/* -- EVM smart-contract interaction ------------------------------------------ */

/** ABI-encoded calldata for ERC-20 transfer(address,uint256): to is 20 bytes. */
AVIARY_API AviaryBufferResult aviary_eth_erc20_transfer(
    const uint8_t* to20, uint64_t amount);

/** ABI 4-byte function selector for a signature, e.g. "balanceOf(address)". */
AVIARY_API AviaryBufferResult aviary_eth_selector(const char* signature);

/** Sign an EVM legacy (EIP-155) transaction with a 32-byte secp256k1 key.
 *  to20 may be NULL for contract creation (pass has_to=0). data may be NULL.
 *  Returns the raw signed transaction (RLP), ready for eth_sendRawTransaction. */
AVIARY_API AviaryBufferResult aviary_eth_sign_legacy(
    uint64_t nonce, uint64_t gas_price, uint64_t gas_limit,
    const uint8_t* to20, int has_to, uint64_t value,
    const uint8_t* data, size_t data_len, uint64_t chain_id,
    const uint8_t* secret32);

/** Free a buffer returned by this library. Safe on a zeroed/NULL buffer. */
AVIARY_API void aviary_buffer_free(AviaryBuffer* buf);

/** Free an error string returned by this library. */
AVIARY_API void aviary_str_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  /* AVIARY_C_H */
