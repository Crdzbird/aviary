# cryptolib BBS pseudonym + blind-issuance primitives — feature request

> What blockchainer needs added to **cryptolib's C ABI** (`bridge/cryptolib_c.h`
> + impl) to unblock the **CredentialGate** anonymous-membership feature
> (`docs/design/CREDENTIAL_GATE.md`). Follows cryptolib's existing conventions:
> `CryptoBuffer`/`CryptoBufferResult` (malloc'd, caller frees via
> `cryptolib_buffer_free` / `cryptolib_str_free`), `CryptoKeyPair`, verify returns
> `int` 1/0, inputs are borrowed `const` pointers, message vectors cross as the
> parallel `msgs[i]` (pointer) + `msg_lens[i]` (length) × `msg_count` arrays
> already used by `cryptolib_bbs_*`.
>
> **Why this is upstream, not in blockchainer.** blockchainer's architecture rule
> is *never reimplement crypto* — it is a consumer of vetted primitives. The two
> asks below are standard CFRG constructions with reference vectors; building
> either as a home-grown composition over the bare-BBS + OPRF surface would mean
> inventing a cross-primitive equality-of-discrete-logs ZK proof, which is exactly
> the class of thing that rule forbids. Putting these in cryptolib keeps the
> soundness burden where the crypto expertise is.
>
> **Verify the exact draft revision, message/proof byte layouts, and KAT values
> against the current CFRG sources when implementing** — the signatures below fix
> the *shape and semantics* blockchainer depends on, not the wire bytes; match
> cryptolib's chosen suite constants and the draft you implement.

---

## Background: what cryptolib exposes today

The current ABI (`cryptolib_c.h` §"BBS Signatures", BLS12-381-SHA-256; sk 32 B,
pk 96 B, signature 80 B) is **bare BBS** —
`cryptolib_bbs_{keygen, sk_to_pk, sign, verify, proof_gen, proof_verify}`:
multi-message signing plus zero-knowledge **selective disclosure**. That is
enough for blockchainer's **Option A** (unlinkable, per-interaction credential
presentation — shippable now on the existing surface).

Bare BBS proofs from one credential are, by design, **mutually unlinkable**. For
a chain that property is the *problem*: it means no Sybil resistance, no targeted
revocation, and no accountability (see `CREDENTIAL_GATE.md` §2, §3). Resolving it
without destroying selective disclosure needs two primitives cryptolib does not
yet expose.

---

## 1. BBS per-verifier linkability (pseudonyms) — the headline blocker

**Draft:** `draft-irtf-cfrg-bbs-per-verifier-linkability` (BBS "pseudonym" mode,
aka BBS#), layered on the base `draft-irtf-cfrg-bbs-signatures` cryptolib already
implements. **Confirm the current revision and its constants.**

The credential is signed over a message vector that includes a dedicated,
never-disclosed **pseudonym secret** `pid` (one of the `msgs`). At presentation
the holder binds a **context** (`verifier_id` — for blockchainer this is
`chain_id ‖ epoch ‖ scope`) and derives a deterministic pseudonym

```
nym = HashToG1(verifier_id) ^ pid          (a single G1 point, ~48 B compressed)
```

together with a proof that `nym` was formed from the *same* `pid` the issuer
signed — in zero knowledge, revealing neither `pid` nor any other hidden
attribute. The essential properties blockchainer relies on:

- **Same holder, same `verifier_id` → identical `nym`** (Sybil resistance within
  an epoch/scope: one credential ⇒ one pseudonym ⇒ one participant).
- **Different `verifier_id` → unlinkable `nym`** (different epoch or scope cannot
  be correlated; cross-epoch privacy preserved).
- All bare-BBS properties (selective disclosure, ZK of hidden attrs,
  issuer-unforgeability, `ph` context binding) are retained.

### Proposed C ABI

```c
/** Deterministically compute a holder's pseudonym for a context, from the
 *  pseudonym-secret scalar `pid` (32 B) that the credential was signed over.
 *  Lets the holder/verifier bookkeep nyms (Sybil cap, revocation) without a
 *  full proof. Returns the pseudonym as a compressed G1 point (48 B). */
CRYPTO_API CryptoBufferResult cryptolib_bbs_calculate_pseudonym(
    const uint8_t* verifier_id, size_t vid_len,
    const uint8_t* pid, size_t pid_len);

/** Selective-disclosure proof AS ABOVE, additionally binding a pseudonym.
 *  `msgs` is the FULL signed vector; `pid_index` (0-based) marks which message
 *  is the pseudonym secret (it MUST NOT also appear in `disclosed_indexes`).
 *  `verifier_id` is the presentation context. Returns the proof bytes; the
 *  caller obtains the matching pseudonym via cryptolib_bbs_calculate_pseudonym
 *  (or, if preferred, expose a variant that returns proof ‖ nym). */
CRYPTO_API CryptoBufferResult cryptolib_bbs_proof_gen_with_pseudonym(
    const uint8_t* pk, size_t pk_len, const uint8_t* signature, size_t sig_len,
    const uint8_t* header, size_t header_len, const uint8_t* ph, size_t ph_len,
    const uint8_t* const* msgs, const size_t* msg_lens, size_t msg_count,
    const uint64_t* disclosed_indexes, size_t disclosed_count,
    uint64_t pid_index,
    const uint8_t* verifier_id, size_t vid_len);

/** Verify a pseudonym-bound proof. `disclosed_msgs`/`disclosed_indexes` are the
 *  revealed subset (excluding the pseudonym secret); `nym` is the claimed
 *  pseudonym (48 B) and `verifier_id` the context it must be bound to. Checks
 *  the proof AND that `nym` is the correct pseudonym for `verifier_id` under the
 *  signed pid. Returns 1 valid, 0 invalid. */
CRYPTO_API int cryptolib_bbs_proof_verify_with_pseudonym(
    const uint8_t* pk, size_t pk_len, const uint8_t* proof, size_t proof_len,
    const uint8_t* header, size_t header_len, const uint8_t* ph, size_t ph_len,
    const uint8_t* const* disclosed_msgs, const size_t* disclosed_lens,
    size_t disclosed_count,
    const uint64_t* disclosed_indexes, size_t indexes_count,
    const uint8_t* nym, size_t nym_len,
    const uint8_t* verifier_id, size_t vid_len);
```

Notes:
- The pseudonym-bound proof is **longer** than a bare `proof_gen` proof (extra
  commitments for the `nym` relation); its length follows the draft — callers
  treat proof bytes as opaque, so no fixed size is assumed.
- If the draft you implement carries the `nym` *inside* the proof rather than
  recomputing it verifier-side, returning `proof ‖ nym` from `proof_gen_*` and
  parsing `nym` out in `proof_verify_*` is equally acceptable — blockchainer only
  needs (a) to obtain the `nym` bytes for its Sybil-cap / deny-list bookkeeping
  and (b) a single verify call that jointly checks proof + pseudonym.

**Priority: HIGH — this is the sole blocker for CredentialGate Option B**
(per-epoch pseudonym → real Sybil resistance + targeted revocation). With it,
Option B lands as a thin blockchainer wrapper; without it, it cannot be built
safely at all.

---

## 2. BBS blind-commitment issuance — removes the trusted-issuer limitation

**Draft:** `draft-irtf-cfrg-blind-bbs-signatures`. **Confirm the current
revision.**

Today `cryptolib_bbs_sign` signs a **plaintext** message vector, so in
blockchainer's issuance flow the `member_secret` / `pid` would have to reach the
issuer in the clear (`CREDENTIAL_GATE.md` §5, §10.2). That is acceptable only in
the trusted-issuer (consortium) model. Blind issuance lets the holder **commit**
to its secret messages so the issuer signs over them **without learning them**.

### Proposed C ABI

```c
/** Holder side: commit to the messages the issuer must NOT see. Returns the
 *  commitment-with-proof blob to send to the issuer, and (separately) the
 *  secret prover blind the holder keeps to finalize/prove later. Suggest a
 *  small struct or a fixed-layout blob carrying both; match cryptolib's style. */
CRYPTO_API CryptoBufferResult cryptolib_bbs_commit(
    const uint8_t* const* committed_msgs, const size_t* committed_lens,
    size_t committed_count,
    /* out */ CryptoBuffer* secret_prover_blind);

/** Issuer side: sign over the holder's commitment plus the issuer's own known
 *  messages → an 80-byte signature the holder later finalizes with its blind. */
CRYPTO_API CryptoBufferResult cryptolib_bbs_blind_sign(
    const uint8_t* sk, size_t sk_len, const uint8_t* pk, size_t pk_len,
    const uint8_t* commitment_with_proof, size_t commit_len,
    const uint8_t* header, size_t header_len,
    const uint8_t* const* msgs, const size_t* msg_lens, size_t msg_count);
```

with the corresponding blind-`proof_gen` / `proof_verify` variants (or a note
that the existing `proof_gen`/`proof_verify` plus the pseudonym functions in §1
compose with a blind-issued signature once the holder has finalized it — whichever
matches the draft's construction).

**Priority: MEDIUM — improves the threat model but is not a hard blocker.**
blockchainer v1 ships CredentialGate under the documented trusted-issuer model
(§10.2); deployments whose threat model includes a *malicious issuer* need this
before they can adopt the gate. Land it after §1.

---

## What this unblocks, and the order blockchainer will consume it

1. **Now, no cryptolib change:** thin `crypto::bbs_*` wrappers over the six bare
   functions + **Option A** unlinkable submitter/gossip admission
   (`CREDENTIAL_GATE.md` §12.1–2). Shippable on the existing ABI.
2. **On §1 (pseudonyms):** **Option B** — per-epoch verifiable pseudonym →
   Sybil-resistant membership + targeted per-epoch `nym` revocation. Thin wrapper
   the day the primitive exists.
3. **On §2 (blind issuance):** malicious-issuer-safe issuance; drops the §10.2
   trusted-issuer caveat.

Anonymous **validators** stay out of scope regardless (they need epoch /
validator-set-change + slashing machinery first — `CREDENTIAL_GATE.md` §3.4).

## Acceptance / KATs requested

- **§1:** same `(pid, verifier_id)` → identical `nym`; distinct `verifier_id` →
  distinct, unlinkable `nym`; a pseudonym-bound proof verifies and leaks nothing
  about `pid` or other hidden attrs; a `nym` swapped for a wrong context fails;
  a proof replayed under a different `verifier_id` / `ph` fails. Ship the draft's
  reference test vectors if available.
- **§2:** the issuer's `blind_sign` output, once finalized by the holder, both
  `bbs_verify`s and drives a valid (pseudonym-bound) `proof_gen`/`proof_verify`;
  the issuer cannot recover a committed message from the commitment.
- Constant-time in secret inputs (`pid`, `sk`, `member_secret`), consistent with
  the rest of cryptolib.
