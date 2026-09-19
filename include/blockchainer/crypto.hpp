#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <optional>
#include <vector>

/// The crypto seam (ARCHITECTURE §3.2). Wraps cryptolib's C ABI in
/// domain-separated, intent-named C++ operations returning real C++ types, so
/// the C shape (lengths, out-pointers, buffer frees) never leaks past here.
/// Consensus and ledger code depend on this seam, never on cryptolib directly.
namespace blockchainer::crypto {

/// A 256-bit digest — the chain's canonical hash width.
using Hash = std::array<std::uint8_t, 32>;

/// Domain-separation tags (docs/CODEC_SPEC.md). A 1-byte prefix so a leaf can
/// never collide with an internal node or a header.
enum class Domain : std::uint8_t {
  kLeaf = 0x00,
  kNode = 0x01,
  kHeader = 0x02,
  kTxId = 0x03,
  kValidatorSet = 0x04,  ///< commitment to a validator set (distinct from a header)
  kGenesis = 0x05,       ///< commitment to a genesis spec
  kProposer = 0x06,      ///< stake-weighted proposer-selection ticket (PoS)
  kPeerRecord = 0x07,    ///< self-signed peer-discovery record (PEX)
  kCredentialPolicy = 0x08,        ///< commitment to a membership-credential policy
  kCredentialPresentation = 0x09,  ///< binds a credential presentation to its context
  kChainBlueprint = 0x0A,          ///< commitment to a chain's component mixture
  kNoiseBinding = 0x0B,            ///< PQ identity binding over a Noise XX handshake hash
  kAnchor = 0x0C,                  ///< a subchain anchor (child finalized headers) in a parent block
  kSubchainRegistry = 0x0D,        ///< commitment to a parent's registry of child tips
  kSubchainPolicy = 0x0E,          ///< commitment to a parent's subchain policy (genesis)
  kParentBinding = 0x0F,           ///< a child's genesis binding to its parent chain
};

/// BLAKE3-256 over [data, data+len). The chain's primary hash primitive.
/// Throws std::runtime_error if the backend reports failure.
Hash blake3_256(const std::uint8_t* data, std::size_t len);

/// Convenience overload over a byte vector.
Hash blake3_256(const std::vector<std::uint8_t>& data);

/// Domain-separated hash: BLAKE3-256( tag ‖ data ).
Hash hash_tagged(Domain tag, const std::vector<std::uint8_t>& data);

/// Internal Merkle node hash: BLAKE3-256( 0x01 ‖ left ‖ right ).
Hash hash_node(const Hash& left, const Hash& right);

/// All-zero digest (used as the conventional empty/sentinel hash).
Hash zero_hash();

// ── Signatures (Ed25519; ARCHITECTURE §3) ────────────────────────────────────

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

/// [n] cryptographically secure random bytes. For transport-layer nonces
/// (challenges, session salts) — NEVER for consensus-critical logic, which must
/// stay deterministic. Throws std::runtime_error on backend failure.
Bytes random_bytes(std::size_t n);

struct KeyPair {
  Bytes public_key;  ///< 32 bytes
  Bytes secret_key;  ///< 64 bytes
};

/// Fresh Ed25519 keypair.
KeyPair ed25519_keygen();

/// Deterministic Ed25519 keypair from a 32-byte seed.
KeyPair ed25519_keygen_from_seed(ByteView seed);

/// Ed25519 signature (64 bytes) over [msg]. Throws on backend failure.
Bytes ed25519_sign(ByteView msg, ByteView secret_key);

/// Verifies an Ed25519 signature. Never throws — returns false on any failure.
bool ed25519_verify(ByteView msg, ByteView sig, ByteView public_key);

/// Account address derived from a public key: BLAKE3-256(public_key).
Hash address_of(ByteView public_key);

// ── Hybrid signature: Ed25519 + ML-DSA-65 (post-quantum tx path) ─────────────
// Keys and signatures are concatenations of the classical then PQ parts
// (ed25519_part ‖ ml_dsa_part). BOTH parts must verify — a "harvest now, forge
// later" quantum adversary that breaks Ed25519 still faces ML-DSA, and a
// downgrade that strips the PQ part fails verification.

/// Hybrid keypair. public_key = ed_pub(32) ‖ mldsa_pub; secret_key = ed_sec ‖ mldsa_sec.
KeyPair hybrid_sig_keygen();

/// Hybrid signature over [msg]: ed_sig(64) ‖ mldsa_sig. Throws on backend failure.
Bytes hybrid_sig_sign(ByteView msg, ByteView secret_key);

/// Verifies a hybrid signature (both parts must pass). Never throws.
bool hybrid_sig_verify(ByteView msg, ByteView sig, ByteView public_key);

// ── ECVRF (RFC 9381, ECVRF-EDWARDS25519-SHA512-TAI) ──────────────────────────
// A verifiable random function: sk maps an input to a unique, unpredictable
// 64-byte output plus an 80-byte proof anyone can verify with pk. The basis for
// grind-proof leader election: the output is fixed the moment (sk, input) is —
// nobody can search for a favourable one — yet unpredictable until revealed.

/// Fresh VRF keypair (pk 32 B, sk = 32-byte seed).
KeyPair ecvrf_keygen();
/// Derive the public key from a 32-byte secret seed. Throws on failure.
Bytes ecvrf_public_key(ByteView sk);
/// The 80-byte proof pi for (sk, alpha). Throws on failure.
Bytes ecvrf_prove(ByteView sk, ByteView alpha);
/// The 64-byte VRF output beta for a proof. Throws on malformed proof.
Bytes ecvrf_proof_to_hash(ByteView pi);
/// Verify [pi] for (pk, alpha): the 64-byte beta on success, std::nullopt on an
/// invalid proof. Never throws.
std::optional<Bytes> ecvrf_verify(ByteView pk, ByteView alpha, ByteView pi);

// ── PQ forward-secret ratchet (hybrid-KEM Double Ratchet) ────────────────────
// A live channel with forward secrecy AND post-compromise security, all
// post-quantum: a leaked message key exposes neither past nor future messages,
// and a full state leak self-heals as the ratchet turns. Stateful (owns an
// opaque handle). The handshake is prekey-bootstrapped — the responder
// publishes a hybrid-KEM prekey, the initiator encapsulates to it — which
// authenticates the RESPONDER (only the prekey holder can open the channel);
// mutual authentication is layered on top by [p2p::ArmoredChannel].
class Ratchet {
 public:
  /// A responder's prekey: a hybrid-KEM keypair. Publish the public half
  /// (authenticated, e.g. in a signed peer record); keep the secret to accept.
  static KeyPair generate_prekey();

  /// Initiator: begin a session to [responder_prekey_pub]. Returns the open
  /// session and the handshake message to hand the responder. Throws on failure.
  static std::pair<Ratchet, Bytes> initiate(ByteView responder_prekey_pub);

  /// Responder: accept a [handshake] with your prekey (public + secret). Throws
  /// on a malformed handshake.
  static Ratchet accept(ByteView handshake, ByteView prekey_pub, ByteView prekey_sec);

  /// Encrypt the next outgoing message (advances the sending ratchet). [aad] is
  /// bound in and must match on decrypt. Throws on failure.
  Bytes encrypt(ByteView plaintext, ByteView aad = {});
  /// Decrypt an incoming message (handles ratchet turns + out-of-order).
  /// Throws on tamper / wrong key / wrong aad / replay.
  Bytes decrypt(ByteView frame, ByteView aad = {});

  bool open() const { return h_ != nullptr; }

  Ratchet(Ratchet&&) noexcept;
  Ratchet& operator=(Ratchet&&) noexcept;
  Ratchet(const Ratchet&) = delete;
  Ratchet& operator=(const Ratchet&) = delete;
  ~Ratchet();

 private:
  explicit Ratchet(void* h) : h_(h) {}
  void* h_ = nullptr;  // CryptoSession
};

// ── Noise XX (Noise_XX_25519_ChaChaPoly_SHA256) ──────────────────────────────
// The standards-based interactive handshake: 3 messages, both static keys are
// transmitted (encrypted) in-band, so neither side needs the other's static key
// up front. Classical (X25519) — its confidentiality is NOT post-quantum; the
// p2p::NoiseSession layers a PQ-hybrid identity binding on top so that
// *authentication* survives a quantum adversary. This is the raw state machine.
// Stateful (owns an opaque handle); not thread-safe.

/// Fresh X25519 keypair (32-byte public, 32-byte secret): a Noise static key.
KeyPair x25519_keygen();

class Noise {
 public:
  /// A Noise XX state for one side. [prologue] must be identical on both sides
  /// (it is mixed into the handshake hash — a mismatch aborts the handshake).
  Noise(bool initiator, ByteView static_pub, ByteView static_sec, ByteView prologue = {});

  /// Produce the next handshake message (this side's turn), embedding [payload].
  Bytes write_message(ByteView payload);
  /// Consume the peer's next handshake message; returns its embedded payload.
  /// Throws on a malformed / tampered / out-of-turn message.
  Bytes read_message(ByteView message);
  /// True once all three handshake messages have been processed.
  bool handshake_finished() const;
  /// The 32-byte handshake hash (channel binding). Both sides agree on it once
  /// the handshake completes; it commits every handshake byte incl. both statics.
  Hash handshake_hash() const;
  /// The peer's static X25519 public key learned in-band. Throws before known.
  Bytes remote_static() const;
  /// Derive the transport cipher states (Noise Split). Throws if the handshake
  /// is not finished or split was already done.
  void split();
  /// Transport record (after split): ciphertext ‖ tag. Sequential nonces — the
  /// receiver must decrypt in order. Throws on failure.
  Bytes encrypt(ByteView plaintext, ByteView ad = {});
  Bytes decrypt(ByteView record, ByteView ad = {});

  Noise(Noise&&) noexcept;
  Noise& operator=(Noise&&) noexcept;
  Noise(const Noise&) = delete;
  Noise& operator=(const Noise&) = delete;
  ~Noise();

 private:
  void* h_ = nullptr;  // CryptoNoiseHandle
};

// ── SLH-DSA (FIPS 205, stateless hash-based) ─────────────────────────────────
// The most conservative PQ family: security reduces to the hash function alone
// (no lattice assumption), so it survives even if ML-DSA's assumptions fall.
// Fixed profile: SLH-DSA-SHA2-128s — the "s"mall variant minimises the on-chain
// signature (7856 B; pk 32 B) at the cost of slower signing, the right trade
// for transactions, which are signed once and stored forever.

/// Fresh SLH-DSA-SHA2-128s keypair (pk 32 B).
KeyPair slh_dsa_keygen();
/// SLH-DSA signature (7856 B) over [msg]. Throws on backend failure.
Bytes slh_dsa_sign(ByteView msg, ByteView secret_key);
/// Verifies an SLH-DSA signature. Never throws — false on any failure.
bool slh_dsa_verify(ByteView msg, ByteView sig, ByteView public_key);

// ── OPAQUE aPAKE (draft-irtf-cfrg-opaque, OPAQUE-3DH, ristretto255-SHA-512) ───
// A password-authenticated key exchange where the password NEVER leaves the
// client and the server stores no password-equivalent. Two phases: a one-time
// registration, then a 3DH login that yields a shared session key iff the
// password is correct. The client-derived `export_key` is stable across logins
// for the same password — a client can encrypt secrets under it that the server
// stores but cannot read.

/// Derive a ristretto255 keypair from a seed (the server's long-term OPAQUE key,
/// and the OPRF keypair OPAQUE builds on).
KeyPair oprf_derive_keypair(ByteView seed, ByteView info = {});

/// Client registration step 1: blind the password. Keep [blind], send [request].
struct OpaqueRegRequest {
  Bytes blind;    ///< secret; feed to opaque_finalize_request
  Bytes request;  ///< send to the server
};
OpaqueRegRequest opaque_registration_request(ByteView password);

/// Server registration step: the 64-byte response for the client to finalize.
Bytes opaque_registration_response(ByteView request, ByteView server_public_key,
                                   ByteView credential_id, ByteView oprf_seed);

/// Client registration step 2: the record (store server-side) + export_key.
struct OpaqueRecord {
  Bytes record;      ///< 192 B — upload to the server
  Bytes export_key;  ///< 64 B — client-side, stable per password
};
OpaqueRecord opaque_finalize_request(ByteView password, ByteView blind, ByteView response,
                                     ByteView server_identity = {}, ByteView client_identity = {});

/// Client login step 1: KE1 (send) + client_state (keep).
struct OpaqueKe1 {
  Bytes ke1;
  Bytes client_state;
};
OpaqueKe1 opaque_client_init(ByteView password);

/// Server login step 1: KE2 (send) + server_state (keep).
struct OpaqueKe2 {
  Bytes ke2;
  Bytes server_state;
};
OpaqueKe2 opaque_server_respond(ByteView context, ByteView server_private_key,
                                ByteView server_public_key, ByteView record, ByteView credential_id,
                                ByteView oprf_seed, ByteView ke1, ByteView server_identity = {},
                                ByteView client_identity = {});

/// Client login step 2: authenticate the server + agree the session key. Throws
/// on a wrong password or server-auth failure.
struct OpaqueLogin {
  Bytes ke3;          ///< send to the server
  Bytes session_key;  ///< 64 B — shared
  Bytes export_key;   ///< 64 B — same as at registration
};
OpaqueLogin opaque_client_finish(ByteView client_state, ByteView ke2, ByteView context,
                                 ByteView server_identity = {}, ByteView client_identity = {});

/// Server login step 2: verify KE3 → the shared session key. Throws on failure.
Bytes opaque_server_finish(ByteView server_state, ByteView ke3);

// ── BBS signatures (draft-irtf-cfrg-bbs-signatures, BLS12-381-SHA-256) ────────
// Multi-message signatures with zero-knowledge SELECTIVE DISCLOSURE: an issuer
// signs a vector of attributes; a holder later proves possession of the whole
// credential while revealing only a chosen subset, in zero knowledge of the
// rest. The privacy primitive behind anonymous credentials. Sizes: sk 32 B,
// pk 96 B, signature 80 B. Two proofs from ONE credential are mutually
// UNLINKABLE — a feature for pure privacy, a design constraint for a chain
// (Sybil/revocation); see docs/design/CREDENTIAL_GATE.md.

/// BBS keypair from key material (>= 32 B) + optional key info. pk 96 B, sk 32 B.
KeyPair bbs_keygen(ByteView key_material, ByteView key_info = {});
/// Derive the 96-byte public key from a 32-byte BBS secret key. Throws on failure.
Bytes bbs_sk_to_pk(ByteView sk);
/// Sign the [msgs] vector under [sk]/[pk] with a domain [header] → 80-byte
/// signature. Throws on backend failure.
Bytes bbs_sign(ByteView sk, ByteView pk, ByteView header, const std::vector<Bytes>& msgs);
/// Verify a [signature] over the [msgs] vector. Never throws — false on any failure.
bool bbs_verify(ByteView pk, ByteView signature, ByteView header,
                const std::vector<Bytes>& msgs);
/// Derive a selective-disclosure proof over the FULL [msgs] vector, revealing
/// only [disclosed_indexes] (0-based, ascending). [ph] (presentation header)
/// binds the proof to a challenge/context. Throws on failure.
Bytes bbs_proof_gen(ByteView pk, ByteView signature, ByteView header, ByteView ph,
                    const std::vector<Bytes>& msgs,
                    const std::vector<std::uint64_t>& disclosed_indexes);
/// Verify a selective-disclosure proof: [disclosed_msgs] aligned with
/// [disclosed_indexes]. Never throws — false on any failure.
bool bbs_proof_verify(ByteView pk, ByteView proof, ByteView header, ByteView ph,
                      const std::vector<Bytes>& disclosed_msgs,
                      const std::vector<std::uint64_t>& disclosed_indexes);

// ── BBS per-verifier pseudonyms + blind issuance ─────────────────────────────
// draft-irtf-cfrg-bbs-per-verifier-linkability-02. The pseudonym seed is
// committed BLINDLY at issuance (the issuer never learns it), so a holder derives
// a deterministic, per-context pseudonym: same holder + same context → the SAME
// pseudonym (Sybil-resistant within a scope), different context → an unlinkable
// one. This is the primitive CredentialGate Option B is built on. A pseudonym is
// a 48-byte compressed G1 point.
//
// SCALAR CONSTRAINT: every nym / entropy scalar (prover_nyms, signer_nym_entropy,
// nym_secrets) is a 32-byte BIG-ENDIAN value that MUST be canonical — strictly
// less than the BLS12-381 scalar-field order r. The backend does NOT reduce them,
// so a non-canonical scalar silently breaks the round-trip. Derive nym seeds so
// they land in [0, r) (e.g. a hash-to-scalar, or clearing enough high bits).

/// A blind commitment: [commitment] (send to the issuer) + [secret_prover_blind]
/// (32 B, the holder keeps to finalize / prove).
struct BbsCommitment {
  Bytes commitment;
  Bytes secret_prover_blind;
};

/// Holder: commit to [committed_msgs] + [prover_nyms] (scalars the issuer must
/// not learn). Throws on backend failure.
BbsCommitment bbs_commit_with_nym(const std::vector<Bytes>& committed_msgs,
                                  const std::vector<Bytes>& prover_nyms);

/// Issuer: blind-sign the [commitment] + own [msgs], mixing [signer_nym_entropy]
/// into the last nym slot. [length_nym_vector] = number of nym scalars. → 80-byte
/// signature. Throws on failure.
Bytes bbs_blind_sign_with_nym(ByteView sk, ByteView pk, ByteView commitment, ByteView header,
                              const std::vector<Bytes>& msgs, ByteView signer_nym_entropy,
                              std::uint64_t length_nym_vector);

/// Holder: finalize the nym secrets = [prover_nyms] with the last element +=
/// [signer_nym_entropy]. Returns one 32-byte scalar per nym. Throws on failure.
std::vector<Bytes> bbs_finalize_nym_secrets(const std::vector<Bytes>& prover_nyms,
                                            ByteView signer_nym_entropy);

/// The deterministic 48-byte pseudonym for [context_id] under [nym_secrets].
/// Throws on failure.
Bytes bbs_calculate_pseudonym(ByteView context_id, const std::vector<Bytes>& nym_secrets);

/// A pseudonym-bound selective-disclosure proof + the 48-byte pseudonym it binds.
struct BbsPseudonymProof {
  Bytes proof;
  Bytes pseudonym;
};

/// Holder: a pseudonym-bound proof over [signer_msgs] + [committed_msgs] for
/// [context_id], revealing the chosen signer / committed indexes (each 0-based
/// into its own vector). Throws on failure.
BbsPseudonymProof bbs_proof_gen_with_pseudonym(
    ByteView pk, ByteView signature, ByteView header, ByteView ph, ByteView context_id,
    const std::vector<Bytes>& signer_msgs, const std::vector<Bytes>& committed_msgs,
    ByteView secret_prover_blind, const std::vector<Bytes>& nym_secrets,
    const std::vector<std::uint64_t>& disclosed_signer_indexes,
    const std::vector<std::uint64_t>& disclosed_committed_indexes);

/// Verifier: check a pseudonym-bound proof. [disclosed_msgs]/[disclosed_indexes]
/// are the COMBINED signer+committed disclosures — a committed index j is passed
/// as j + [L] + 1, where [L] is the number of signer messages;
/// [length_nym_vector] is the number of nym scalars. Never throws — false on any
/// failure.
bool bbs_proof_verify_with_pseudonym(ByteView pk, ByteView proof, ByteView header, ByteView ph,
                                     ByteView context_id, ByteView pseudonym, std::uint64_t L,
                                     std::uint64_t length_nym_vector,
                                     const std::vector<Bytes>& disclosed_msgs,
                                     const std::vector<std::uint64_t>& disclosed_indexes);

// Standalone blind issuance (draft-irtf-cfrg-bbs-blind-signatures-02, no
// pseudonyms): the holder hides [committed_msgs] from the signer.

/// Holder: commit to [committed_msgs] (the signer never learns them).
BbsCommitment bbs_blind_commit(const std::vector<Bytes>& committed_msgs);
/// Signer: blind-sign the [commitment] + own [msgs] → 80-byte signature.
Bytes bbs_blind_sign(ByteView sk, ByteView pk, ByteView commitment, ByteView header,
                     const std::vector<Bytes>& msgs);
/// Verify a blind signature over [msgs] + [committed_msgs] with the holder's
/// [secret_prover_blind]. Never throws — false on any failure.
bool bbs_verify_blind_sign(ByteView pk, ByteView signature, ByteView header,
                           const std::vector<Bytes>& msgs, const std::vector<Bytes>& committed_msgs,
                           ByteView secret_prover_blind);

/// Deterministically map [msg] (+ domain [dst]) to a canonical 32-byte scalar in
/// [0, r) — the safe way to satisfy the SCALAR CONSTRAINT above. A stable
/// per-holder nym seed is `bbs_hash_to_scalar(member_secret, dst)`, so the same
/// member gets the same per-context pseudonym across presentations. Throws on
/// backend failure.
Bytes bbs_hash_to_scalar(ByteView msg, ByteView dst = {});
/// A fresh cryptographically-random canonical 32-byte scalar in [0, r). Throws on
/// backend failure.
Bytes bbs_random_scalar();

// ── BLS12-381 (consensus votes; ARCHITECTURE §7) ─────────────────────────────

/// Deterministic BLS keypair from input key material (>= 32 bytes).
/// secret_key = 32 bytes, public_key = 48 bytes.
KeyPair bls_keygen_from_ikm(ByteView ikm);

/// BLS signature (96-byte compressed G2) over [msg]. Throws on backend failure.
Bytes bls_sign(ByteView msg, ByteView secret_key);

/// Verifies a single BLS signature. Never throws.
bool bls_verify(ByteView msg, ByteView sig, ByteView public_key);

/// Aggregates N BLS signatures (each 96 bytes) into one. Throws if empty.
Bytes bls_aggregate(const std::vector<Bytes>& sigs);

/// Verifies an aggregate signature over N (message, public key) pairs. For BFT
/// votes the messages are identical (the same block id under many keys) — pass
/// the message repeated. Never throws.
bool bls_aggregate_verify(const std::vector<Bytes>& msgs,
                          const std::vector<Bytes>& pubkeys, ByteView agg_sig);

// ── Hybrid KEM + SecretStream (secure P2P channel; ARCHITECTURE §9) ──────────

/// BLAKE3 key derivation with a domain-separation context string. 32-byte key.
Bytes derive_key(const std::string& context, ByteView ikm);

struct KemEncaps {
  Bytes ciphertext;
  Bytes shared_secret;  ///< 32 bytes
};

/// Hybrid KEM (X25519 + ML-KEM-768) keypair (concatenated layout). Secure while
/// either the classical or the post-quantum part is unbroken.
KeyPair hybrid_kem_keygen();

/// Encapsulate to [public_key]: returns ciphertext + 32-byte shared secret.
KemEncaps hybrid_kem_encapsulate(ByteView public_key);

/// Decapsulate [ciphertext] with [secret_key]: returns the 32-byte shared secret.
Bytes hybrid_kem_decapsulate(ByteView ciphertext, ByteView secret_key);

// ── Recipient-bound PQ seals (one-call; self-describing envelope) ────────────
// Encapsulate to a recipient's hybrid-KEM public key, then MolecularVault-seal
// under the shared secret. Only the holder of the KEM secret can open — no
// shared passphrase to phish. The signed variant also authenticates the sender.

/// Seal [plaintext] to [recipient_kem_public] (with optional [aad]). Throws on
/// failure.
Bytes pq_seal_to(ByteView plaintext, ByteView recipient_kem_public, ByteView aad = {});
/// Open a [pq_seal_to] envelope with [recipient_kem_secret]. Throws on tamper /
/// wrong key / wrong aad.
Bytes pq_open(ByteView envelope, ByteView recipient_kem_secret, ByteView aad = {});

/// Seal [plaintext] to [recipient_kem_public] AND sign it with
/// [signer_sig_secret] (hybrid signature). Throws on failure.
Bytes pq_seal_signed_to(ByteView plaintext, ByteView recipient_kem_public,
                        ByteView signer_sig_secret, ByteView aad = {});
/// Open a [pq_seal_signed_to] envelope: returns the plaintext ONLY if it
/// decrypts under [recipient_kem_secret] AND the signature verifies under
/// [signer_sig_public]. Throws otherwise.
Bytes pq_open_signed(ByteView envelope, ByteView recipient_kem_secret, ByteView signer_sig_public,
                     ByteView aad = {});

/// Forward-secure stream encryptor (libsodium SecretStream). RAII over the
/// native handle; movable, non-copyable.
class StreamEncryptor {
 public:
  explicit StreamEncryptor(ByteView key32);
  ~StreamEncryptor();
  StreamEncryptor(StreamEncryptor&&) noexcept;
  StreamEncryptor& operator=(StreamEncryptor&&) noexcept;
  StreamEncryptor(const StreamEncryptor&) = delete;
  StreamEncryptor& operator=(const StreamEncryptor&) = delete;

  /// 24-byte stream header; must reach the decryptor before any frame.
  const Bytes& header() const { return header_; }
  /// Encrypt one frame. [final_chunk] marks the end of the stream.
  Bytes push(ByteView plaintext, bool final_chunk = false);

 private:
  void* h_ = nullptr;
  Bytes header_;
};

// ── Interop primitives (EVM / Bitcoin / Solana; ARCHITECTURE §11) ────────────

/// SHA-256 (Bitcoin, and general use).
Hash sha256(ByteView msg);

/// Keccak-256 (original Keccak padding — Ethereum). NOT NIST SHA3-256.
Hash keccak256(ByteView msg);

/// RIPEMD-160 (20-byte output). Bitcoin HASH160(x) = ripemd160(sha256(x)).
Bytes ripemd160(ByteView msg);

/// secp256k1 keypair: sk = 32 bytes, pk = 65 bytes uncompressed (0x04‖X‖Y).
KeyPair secp256k1_keygen();

/// Public key from a 32-byte secret key. compressed → 33 bytes, else 65.
Bytes secp256k1_pubkey(ByteView secret_key, bool compressed);

/// ECDSA sign a 32-byte digest (RFC6979, low-S). Returns 65 bytes:
/// r(32) ‖ s(32) ‖ recovery_id(1). Throws on failure.
Bytes secp256k1_sign(ByteView digest32, ByteView secret_key);

/// Verify a secp256k1 signature. [sig] is 64 (r‖s) or 65 bytes; [pubkey] 33/65.
bool secp256k1_verify(ByteView digest32, ByteView sig, ByteView pubkey);

/// Recover the 65-byte uncompressed public key from a 32-byte digest and a
/// 65-byte recoverable signature (r‖s‖recovery_id). Ethereum ecrecover.
Bytes secp256k1_recover(ByteView digest32, ByteView sig65);

/// Forward-secure stream decryptor. Throws on authentication failure.
class StreamDecryptor {
 public:
  StreamDecryptor(ByteView key32, ByteView header24);
  ~StreamDecryptor();
  StreamDecryptor(StreamDecryptor&&) noexcept;
  StreamDecryptor& operator=(StreamDecryptor&&) noexcept;
  StreamDecryptor(const StreamDecryptor&) = delete;
  StreamDecryptor& operator=(const StreamDecryptor&) = delete;

  /// Decrypt one frame; sets [*is_final] if non-null. Throws std::runtime_error
  /// on tamper / wrong order / truncation.
  Bytes pull(ByteView ciphertext, bool* is_final = nullptr);

 private:
  void* h_ = nullptr;
};

/// Lowercase hex encoding of a hash (no prefix). Test/debug helper.
std::string to_hex(const Hash& h);

}  // namespace blockchainer::crypto
