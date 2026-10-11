# CredentialGate - Design Doc

> **Status: DESIGN - not implemented.** This is the contract we review before
> writing code. The reason `consensus::CredentialGate` gets a design doc when the
> other assurance-plan classes went straight to implementation: BBS anonymous
> credentials collide head-on with three things a blockchain needs - Sybil
> resistance, revocation, and slashing accountability. Those collisions are
> protocol decisions, not glue, and getting them wrong ships a privacy feature
> that quietly breaks safety. This doc resolves them first.
>
> Author: assurance-plan Phase 3. Depends on cryptolib BBS
> (`draft-irtf-cfrg-bbs-signatures`, BLS12-381-SHA-256) + OPAQUE/OPRF already in
> the ABI. See [`../ARCHITECTURE.md`](../ARCHITECTURE.md) section 9 and the
> `AssuranceProfile` in [`../IMPROVEMENTS.md`](../IMPROVEMENTS.md).

---

## 0. One-paragraph summary

`CredentialGate` lets a **permissioned** chain admit participants (tx submitters,
gossip peers, and - carefully - validators) by a **zero-knowledge proof that they
hold a membership credential** issued by the chain's authority, revealing only
the attributes a policy requires (e.g. `tier >= 2`) and nothing else. It is built
on BBS selective disclosure, with the **issuer public key** and **admission
policy** committed into the genesis hash so every node enforces the same gate.
The hard part - and the bulk of this doc - is that naive BBS proofs are
**unlinkable**, which is exactly wrong for Sybil resistance, revocation, and
slashing; the resolution is a **linkable-by-epoch pseudonym** bound into every
proof.

**Bottom line after auditing the cryptolib ABI (section 10):** it exposes only *bare*
BBS - no pseudonym or blind-commitment primitive. So this splits into a
**shippable-now** part (BBS wrappers + *unlinkable* submitter/gossip admission,
Sybil-weak but safe) and a **blocked** part (the per-epoch pseudonym that gives
real Sybil resistance), which depends on an **upstream cryptolib BBS-pseudonym
primitive** - *not* on a home-grown zero-knowledge construction, which the
project's "never reimplement crypto" rule forbids. Validator anonymity is deferred
further, behind slashing/epoch machinery that doesn't exist yet.

---

## 1. Motivation - where it fits

Today membership is all-or-nothing and identifying:

- **Validators** are a fixed `ValidatorSet` of BLS public keys committed at
  genesis. Everyone knows exactly who every validator is.
- **Tx submitters / gossip peers** are gated only by the transport allow-list
  (`PeerRecord` signing keys) - a static, fully-identifying roster.

For a **consortium / permissioned** deployment (banks, a supply-chain network, a
regulated venue) the requirement is different:

> *"Only members of the consortium may submit transactions and join the mesh, and
> a member should be able to prove membership - and a required attribute level - 
> **without revealing which member they are** to their peers or to an on-chain
> observer."*

That is an **anonymous credential** problem. BBS is the standard primitive:
an **issuer** (the consortium authority) signs a member's attribute vector once;
the member later derives, per interaction, a **proof** that discloses only a
chosen subset while proving the whole vector was issuer-signed. The rest of the
attributes - including the member's identity attribute - stay hidden in
zero-knowledge.

This is the missing tier of the "private / permissioned" transport profile the
architecture always promised but never built past a static allow-list.

---

## 2. What BBS gives us (and what it does not)

From the cryptolib ABI (`cryptolib_bbs_*`, BLS12-381-SHA-256; sk 32 B, pk 96 B,
signature 80 B):

- `bbs_sign(sk, pk, header, msgs[])` - the **issuer** signs a *vector* of messages
  (attributes) -> one 80-byte credential.
- `bbs_proof_gen(pk, sig, header, ph, msgs[], disclosed_indexes[])` - the
  **holder** derives a proof revealing only `disclosed_indexes`; the presentation
  header `ph` binds the proof to a challenge/context.
- `bbs_proof_verify(pk, proof, header, ph, disclosed_msgs[], disclosed_indexes[])`
 - any **verifier** checks the proof against the issuer's `pk`.

Properties we get **for free**:

1. **Selective disclosure** - reveal `tier`, hide `member_id`, `region`, `quota`...
2. **Zero-knowledge of undisclosed attributes** - a verifier learns nothing about
   hidden messages beyond "the issuer signed them".
3. **Issuer-unforgeability** - no valid proof without an issuer signature.
4. **`ph` context binding** - a proof made for challenge *A* does not verify
   against challenge *B* (anti-replay, when we choose `ph` well).

Property that is **a problem, not a feature, here**:

5. **Unlinkability** - two proofs from the *same* credential are
   cryptographically **unlinkable**. Wonderful for pure privacy; **fatal** for a
   chain, because it means:
  - **No Sybil resistance** - one credential can back unboundedly many "distinct"
     participants; a single member could masquerade as a whole validator quorum.
  - **No revocation targeting** - you cannot say "ban *that* member" if you can
     never recognise *that* member twice.
  - **No accountability / slashing** - equivocation evidence is worthless if the
     equivocator is unlinkable from block to block.

**section 3 is entirely about resolving property 5 without destroying properties 1-4.**

---

## 3. The core tension: anonymity vs accountability

A blockchain needs, at minimum, to answer *"is this the same participant as
before?"* to resist Sybils, revoke, and slash. Pure BBS refuses to answer it.
Three ways out, in increasing order of what we adopt:

### 3.1 Option A - Full anonymity, submitter-only (partial)
Use unlinkable proofs, but **only** to gate actions that are individually
self-authorising and don't need cross-action identity: **transaction submission**.
Each tx already carries its own signature + nonce/UTXO (double-spend protection
lives in the state machine, not the credential). So a submitter proves "I'm a
member, tier >= k" per tx, unlinkably, and the tx's own signature does the
anti-replay. **This works and is safe** - but it does **not** extend to validators
or to rate-limiting a member, because those need linkability.

### 3.2 Option B - Linkable pseudonym (the chosen core)
Bind a **verifiable, deterministic pseudonym** into every proof:

```
nym = H( member_secret || epoch || scope )
```

where `member_secret` is a *hidden* BBS attribute (attribute 0, never disclosed)
and `epoch`/`scope` are public context. The holder discloses `nym` (not the
secret) and, via the BBS proof, proves in zero-knowledge that `nym` was computed
from the *signed* `member_secret`. Now:

- **Same member, same epoch+scope -> same `nym`.** Sybil-resistant *within* an
  epoch/scope: one credential = one pseudonym = one participant.
- **Different epoch -> different `nym`.** Cross-epoch unlinkable, so long-term
  behavioural tracking is still defeated - privacy is preserved at the
  granularity we choose (`epoch` length is a policy knob).
- **`scope`** separates roles: a member's `nym` as a *gossip peer* differs from
  its `nym` as a *tx submitter*, so the two can't be correlated.

The zero-knowledge "`nym` was derived from the signed secret" step is the one
piece BBS's bare `proof_gen` does **not** give directly - it proves disclosed
*equal* signed values, not *functions* of hidden values. **This is the central
open technical question (see section 10.1):** either (a) a BBS variant / pseudonym
extension (`draft-irtf-cfrg-bbs-per-verifier-linkability` / BBS# "pseudonym"
mode) if cryptolib exposes it, or (b) a construction we build over the primitives
we have (OPRF-based verifiable pseudonym + a disclosed commitment attribute). The
doc's recommendation depends on what cryptolib can provide; **we do not implement
until section 10.1 is answered.**

### 3.3 Option C - Full identity (the fallback we already have)
The status-quo `ValidatorSet` / `PeerRecord` allow-list. No privacy, full
accountability. `CredentialGate` **complements** this, it does not replace it - 
validators may well stay in Option C while *submitters* move to Option A/B.

### 3.4 Decision

| Participant | Gate | Rationale |
|---|---|---|
| **Tx submitter** | Option A (unlinkable) **or** B (per-epoch nym) by policy | tx signature already prevents replay; privacy is cheap here |
| **Gossip peer** | Option B (per-epoch nym, `scope="gossip"`) | needs Sybil resistance + peer-scoring linkage within an epoch |
| **Validator** | Option C (identified) in v1; Option B behind a **future** epoch/slashing redesign | slashing needs stable identity across heights - **out of scope for v1** |

**v1 scope: submitter + gossip admission. Validator anonymity is explicitly
deferred** until the epoch/validator-set-change machinery exists (it is already a
separate `[H]` backlog item), because anonymous validators without linkable
slashing is a safety regression, not a feature.

---

## 4. Credential schema

The issuer signs a fixed-order attribute vector. Order is part of the schema and
committed at genesis (a verifier must agree on index meanings):

| idx | attribute | disclosed? | purpose |
|---|---|---|---|
| 0 | `member_secret` (32 B) | **never** | pseudonym derivation + unforgeable identity |
| 1 | `tier` (u8) | on demand | policy threshold (`tier >= k`) |
| 2 | `not_after` (u64, unix) | **always** | credential expiry (verifier checks) |
| 3 | `issuer_epoch` (u32) | always | which issuance round / key epoch |
| 4... | policy-specific (region, quota class...) | on demand | deployment-defined |

- `member_secret` is generated by the **member**, never by the issuer (blind
  issuance, section 5) - so even the issuer cannot later impersonate the member or link
  its pseudonyms. This matters: it keeps the issuer honest-but-curious-safe.
- `not_after` gives **coarse revocation for free** - expiry. Fine-grained
  revocation is section 6.

The **schema hash** = `hash_tagged(kCredentialSchema, canonical-encode(schema))`
is committed into `GenesisSpec` (section 7) so the attribute layout is chain identity.

---

## 5. Protocol: issuance

Blind, so the issuer never sees `member_secret`:

```
member:  member_secret <- random 32 B
         commitment     <- Commit(member_secret)            (Pedersen/BBS blind)
         -> send {commitment, requested attrs (tier, ...)} to issuer, over an
           ArmoredChannel or RemoteGuard-authenticated admin channel
issuer:  verify the member is entitled to the requested attrs (out-of-band /
         policy) ; assemble the attribute vector with the committed secret ;
         bbs_sign(...) -> credential (80 B)
         -> return credential
member:  bbs_verify(issuer_pk, credential, attrs)          (sanity)
         store {credential, member_secret, attrs} sealed at rest
```

*If cryptolib's BBS does not expose blind-commitment issuance*, the fallback is:
the member sends `member_secret` to the issuer over a `RemoteGuard`/OPAQUE
channel and trusts the issuer not to retain it (a weaker but common model). **This
is open question section 10.2.** Blind issuance is strongly preferred and is why section 10.2
gates implementation.

---

## 6. Protocol: admission (the gate)

A verifier (any node) challenges; the holder presents:

```
verifier -> holder:   challenge = random 32 B          (per-connection nonce)
holder:
  epoch  = current_epoch()                            (public, deterministic)
  scope  = "gossip" | "submit"
  ph     = hash_tagged(kCredentialPresentation,
                       chain_id || epoch || scope || challenge)   <- binds context
  nym    = pseudonym(member_secret, epoch, scope)      (section 3.2)
  proof  = bbs_proof_gen(issuer_pk, credential, header, ph,
                         attrs, disclosed = {tier, not_after, issuer_epoch, nym})
  -> send {proof, disclosed attrs, nym}
verifier:
  1. bbs_proof_verify(issuer_pk, proof, header, ph, disclosed, indexes) == 1
  2. not_after > now                                   (expiry / coarse revocation)
  3. issuer_epoch not in revoked_epochs                     (section 6.1 bulk revocation)
  4. tier >= policy.min_tier                            (attribute policy)
  5. nym not in revoked_nyms  (this epoch)                  (section 6.2 targeted revocation)
  6. nym not already admitted this epoch for this scope (section 6.3 Sybil cap)
  -> admit / reject
```

`ph` binding the `challenge` defeats proof replay across connections; binding
`chain_id || epoch || scope` defeats cross-chain / cross-role / stale-epoch replay.

### 6.1 Bulk revocation - issuer epochs
Rotate the issuer key by `issuer_epoch`. To revoke *a whole issuance batch*, add
its `issuer_epoch` to a committed `revoked_epochs` set (a governance action / a
genesis-for-the-next-epoch value). Cheap, coarse.

### 6.2 Targeted revocation - pseudonym deny-list
Because Option B makes `nym` **stable within an epoch**, a misbehaving member's
`nym` (observed once) can be added to a per-epoch `revoked_nyms` set that all
nodes enforce. The member's *identity* stays hidden; only its epoch-pseudonym is
banned. At the next epoch the `nym` changes - so a *persistent* revocation must
be re-derivable, which requires the issuer to also revoke the credential
(section 6.1) or maintain an accumulator (section 10.3, deferred).

### 6.3 Sybil cap
Within an epoch+scope, a `nym` is unique per member. A verifier (or the gossip
overlay) refuses a second distinct session claiming an already-admitted `nym`,
or rate-limits per `nym`. This is the Sybil resistance Option A cannot give.

---

## 7. Integration & genesis commitment

`CredentialGate` is a **policy object**, mirroring `AssuranceProfile`:

```cpp
struct CredentialPolicy {
  Bytes issuer_pk;                      // 96 B BBS issuer public key
  Hash  schema_hash;                    // section 4 attribute layout
  std::uint8_t min_tier = 0;            // attribute threshold
  std::uint32_t epoch_length = 0;       // heights per pseudonym epoch (0 = disabled)
  std::vector<std::uint32_t> revoked_issuer_epochs;
  // gate mode per role: submit / gossip -> {off, unlinkable, per_epoch_nym}
};
```

- The **policy hash is committed into `GenesisSpec`** (a new optional field, same
  version-gating discipline as `assurance_profile`), so every node enforces the
  same issuer + schema + thresholds - a node cannot silently relax the gate. This
  is the "suite-id-in-the-genesis-hash" pattern again.
- `AssuranceProfile` gains a `credential_gate()` accessor: `standard` -> off,
  `hardened`/`fortress` -> on for gossip admission (a permissioned deployment
  opts in explicitly). *This coupling is tentative - see section 10.4.*
- **Seams it plugs into** (all existing):
 - **PEX / gossip admission** - `AddressBook::merge` / the transport allow-list
    additionally require a valid gossip-scope proof before accepting a peer.
 - **Tx admission** - `Mempool::add` (or a `SealedMempool` sibling) requires a
    submit-scope proof attached to the tx envelope.
- **On-chain footprint: none by default.** The proof gates *admission*, it is not
  stored in blocks. (An *auditable* mode that records `nym`s on-chain for later
  dispute is a deliberate opt-in, off by default, to keep the privacy promise.)

---

## 8. Determinism & version-gating

- **Proofs are non-deterministic** (BBS proof_gen is randomised) and therefore
  **never enter consensus state** - they gate admission only. Blocks and state
  stay byte-identical across nodes, as the determinism mandate requires. This is
  the same discipline used for the transport handshakes and the sealed mempool.
- **Verification is deterministic** (`proof_verify` is a pure predicate), so two
  honest nodes always agree admit/reject given the same policy + inputs.
- Every new wire/format object (`CredentialProof` envelope, `CredentialPolicy`
  encoding) is **version-gated** and added to the **codec fuzz totality sweep**,
  exactly like `SealedTx`, `VrfPosData`, `ShareCard`, `PeerRecord`.
- The **test determinism rule learned from VRF applies**: any test that branches
  on a proof outcome must use fixed-seed credentials, never random keygen, or the
  suite flakes. Proof *bytes* vary per run (randomised) - assert *behaviour*
  (verify true/false), never proof bytes.

---

## 9. Proposed API (once section 10 is resolved)

```cpp
// crypto seam - thin BBS wrappers (mirror the ecvrf/slh_dsa/opaque additions)
namespace crypto {
  KeyPair bbs_keygen(ByteView key_material, ByteView key_info = {});
  Bytes   bbs_sign(ByteView sk, ByteView pk, ByteView header,
                   const std::vector<Bytes>& msgs);
  bool    bbs_verify(ByteView pk, ByteView sig, ByteView header,
                     const std::vector<Bytes>& msgs);
  Bytes   bbs_proof_gen(ByteView pk, ByteView sig, ByteView header, ByteView ph,
                        const std::vector<Bytes>& msgs,
                        const std::vector<std::uint64_t>& disclosed);
  bool    bbs_proof_verify(ByteView pk, ByteView proof, ByteView header, ByteView ph,
                           const std::vector<Bytes>& disclosed_msgs,
                           const std::vector<std::uint64_t>& disclosed_indexes);
}

// consensus/membership policy + the gate
namespace aviary::consensus {
  struct CredentialPolicy { /* section 7; encode()/decode_all()/hash() */ };

  class CredentialGate {
   public:
    explicit CredentialGate(CredentialPolicy policy);
    // Holder side: build a presentation for a challenge in a scope.
    static Bytes present(const Credential& cred, std::uint32_t epoch,
                         std::string_view scope, ByteView challenge);
    // Verifier side: admit? Runs the full section 6 checklist. Returns the disclosed
    // pseudonym on success (for Sybil-cap / revocation bookkeeping), or nullopt.
    std::optional<Bytes> admit(std::uint64_t chain_id, std::uint32_t epoch,
                               std::string_view scope, ByteView challenge,
                               ByteView presentation) const;
    void revoke_nym(const Bytes& nym);
    void revoke_issuer_epoch(std::uint32_t epoch);
  };
}
```

---

## 10. Open questions - MUST answer before implementing

These are why this is a doc and not a commit. Each blocks code. **section 10.1 and section 10.2
were audited against the current cryptolib ABI while writing this doc - findings
recorded inline.**

1. **Verifiable pseudonym over a hidden attribute (section 3.2). - AUDITED: no native
   support.** The cryptolib ABI exposes only **bare** BBS
   (`bbs_{keygen,sk_to_pk,sign,verify,proof_gen,proof_verify}`) - no
   `pseudonym`/`nym`/per-verifier-linkability entry point. So Option B is **not a
   thin wrapper**. The two paths:
  - **(a) Preferred - upstream it.** Request the BBS *pseudonym* extension
     (`draft-irtf-cfrg-bbs-per-verifier-linkability`, aka BBS# pseudonym mode) in
     cryptolib. This puts the ZK-soundness burden where the crypto expertise is
     and keeps aviary a *consumer* of vetted primitives - consistent with
     the whole architecture (we never reimplement crypto).
  - **(b) Fallback - custom construction.** OPRF *is* available
     (`oprf_{blind,blind_evaluate,evaluate,finalize,derive_keypair}`), so a
     verifiable pseudonym `nym = OPRF(member_secret, epoch||scope)` is derivable.
     **But** proving in zero-knowledge that this `nym` uses the *same*
     `member_secret` the BBS credential signed requires a **cross-primitive
     proof-of-equality (equality of discrete logs across a BBS commitment and an
     OPRF evaluation)** - a novel ZK protocol. Per `cpp-crypto-engineer` /
     `security-auditor` discipline, **we do not roll our own ZK linkage.** Path
     (b) is therefore *only* viable if the equality proof is a standard,
     reviewable construction with test vectors - not something we invent.

   -> **Decision: pursue (a). Do not implement Option B until cryptolib exposes a
     reviewed pseudonym primitive.** Option A (unlinkable) is unblocked and ships
     first.
2. **Blind issuance (section 5). - AUDITED: no native BBS blind-commit.** The ABI signs a
   *plaintext* message vector (`bbs_sign` takes raw `msgs`); there is no
   committed/blind-message signing entry point. So `member_secret` would reach the
   issuer in v1 unless we build blinding over OPRF (again a custom composition,
   section 10.1(b)). -> **Decision: v1 uses the trusted-issuer model** (member_secret sent
   to the issuer over a `RemoteGuard`/OPAQUE-authenticated channel; issuer must not
   retain it), documented as a known limitation. Blind issuance follows a cryptolib
   BBS-blind-commit primitive, same as section 10.1(a). Acceptable for consortium
   deployments where the issuer is already the trust root; **not** acceptable if
   the threat model includes a malicious issuer - such deployments must wait for
   (a).
3. **Cryptographic accumulator for persistent revocation (section 6.2).** Per-epoch
   `nym` deny-lists are coarse; a proper revocation accumulator (non-membership
   proof) is the standard answer but is a large addition. Deferred unless a
   deployment requires sub-epoch persistent revocation.
4. **AssuranceProfile coupling (section 7). - RESOLVED: orthogonal.** Credential gating
   is committed as an independent `GenesisSpec::membership_policy` (the canonical
   `CredentialPolicy` bytes; empty = permissionless), NOT a dimension of
   `AssuranceProfile`. The crypto posture and the permissioned/permissionless axis
   are genuinely independent - a chain can be `fortress` *and* permissionless, or
   `standard` *and* permissioned - so folding them would force false combinations.
   `CredentialPolicy::commit_to(GenesisSpec&)` / `from_genesis(...)` mirror
   `AssuranceProfile::apply_to` / `from_genesis`; both fields ride the genesis
   hash side by side (`test_genesis.cpp`).
5. **Epoch source.** `epoch = f(height)` is the obvious deterministic source, but
   couples pseudonym rotation to block production; an explicit governance epoch is
   cleaner but needs the (absent) validator-set-change machinery. -> *v1: derive
   from height with a policy `epoch_length`.*

---

## 11. Test plan (when built)

- **Primitive round-trips**: `bbs_sign`/`verify`; `proof_gen`/`proof_verify` with
  various disclosed subsets; a tampered proof / wrong issuer / wrong `ph` all fail.
- **Selective disclosure**: a proof disclosing `{tier}` verifies and leaks nothing
  about `member_secret` / hidden attrs (structural check on disclosed set).
- **Policy**: `tier >= k` admits/rejects; expired `not_after` rejected; revoked
  `issuer_epoch` rejected.
- **Pseudonym (Option B)**: same member+epoch+scope -> equal `nym`; different epoch
  -> different `nym`; different scope -> different `nym`; a revoked `nym` rejected;
  a second session on an admitted `nym` refused (Sybil cap).
- **Replay**: a presentation for challenge A / epoch E / chain X fails against
  B / E+1 / Y.
- **Integration**: a gossip peer admitted only with a valid gossip-scope proof;
  a submit-scope proof gates `Mempool::add`.
- **Determinism**: fixed-seed credentials -> reproducible admit/reject outcomes
  (proof bytes may vary); the `CredentialPolicy`/`CredentialProof` decoders in the
  fuzz totality sweep.
- ASan/UBSan clean; golden vectors unaffected (no on-chain encoding change).

---

## 12. Recommendation

Given the section 10 audit (bare BBS only; OPRF present; no pseudonym or blind-commit):

1. **Build the crypto BBS wrappers now** (section 9, the six bare functions) - they are
   unambiguous, useful on their own (verifiable multi-attribute credentials with
   selective disclosure), and are the foundation both options need. **This is the
   only part safe to implement today.**
2. **Ship v1 = Option A: unlinkable submitter + gossip admission.** A member
   proves "issuer-signed, `tier >= k`, not expired" per interaction, unlinkably;
   the tx's own signature/nonce handles anti-replay, and gossip peer-scoring falls
   back to per-connection (not per-identity) limits. This is Sybil-*weak* but
   safe, private, and shippable on bare BBS **now**. Document the Sybil caveat.
3. **Block Option B (per-epoch pseudonym -> real Sybil resistance) on cryptolib.**
   File a request for the BBS **pseudonym** extension (section 10.1(a)). Do **not** build
   a custom BBSxOPRF equality-of-discrete-logs ZK proof - that is exactly the
   "roll your own crypto" the project forbids. Option B lands as a thin wrapper the
   day the primitive exists.
4. **Do not gate validators anonymously in v1** (section 3.4) - deferred behind the
   epoch / validator-set-change machinery, because anonymous validators without
   linkable slashing is a safety *downgrade*.
5. **Commit the membership policy into the genesis hash** (section 7) so the gate is
   chain identity, consistent with `AssuranceProfile`.

**Net:** the audit turned "should we build this?" into a clear, staged answer.
The shippable-now core is **BBS wrappers + Option-A unlinkable admission**; the
headline feature (Sybil-resistant anonymous membership) is **correctly blocked on
an upstream cryptolib primitive** rather than on a home-grown ZK protocol. This
keeps aviary a consumer of vetted crypto and avoids the single most
dangerous way this feature could go wrong. Next action is a **cryptolib feature
request for BBS pseudonyms** - **filed** as
[`docs/CRYPTOLIB_BBS_PSEUDONYM_REQUIREMENTS.md`](../CRYPTOLIB_BBS_PSEUDONYM_REQUIREMENTS.md)
(section 1 pseudonyms = the Option-B blocker, section 2 blind issuance = the section 10.2
malicious-issuer fix) - after which section 9 implements in the order of this section.
