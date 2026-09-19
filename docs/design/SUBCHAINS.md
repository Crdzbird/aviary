# Subchains — a block that anchors a whole blockchain — Design Doc

Status: **ACCEPTED (recommendations approved 2026-09-19) — stages 1–3 built.** This document exists because the feature
is delicate: it changes what a block commits to, adds a second trust level, and
touches consensus-critical code. Every question in §9 must be answered before a
line of implementation is written; every invariant in §5 must have a test in
§10 before the feature is enabled on any chain.

Related: `ARCHITECTURE.md` §4 (canonical encoding), §6 (block pipeline), §10
(light client); `design/CREDENTIAL_GATE.md` (the genesis-commitment pattern
this reuses); `IMPROVEMENTS.md` (composable chains, foreign components).

---

## 0. One-paragraph summary

A **subchain** is an ordinary blockchainer chain (any mixture the
`ChainBlueprint` allows) whose finalized progress is **anchored** into a
*parent* chain: a parent block carries, for each child it anchors, the child's
finalized block header plus the child's commit certificate. The parent's
validators verify the certificate against the child's registered validator set
— exactly what a `LightClient` already does — and record the child's new tip in
a registry committed into the parent header. The result is a tree of chains:
each level orders, timestamps, and vouches for the *finality* of the level
below, and anyone holding a parent header can prove a child's state
(`parent.subchain_root → child.state_root → account`). "One block is a
sub-blockchain" is realised as **one block commits to a sub-blockchain's
finalized state**; the parent never re-executes the child.

---

## 1. Motivation — where it fits

- **Horizontal scale without breaking the determinism mandate.** Each child runs
  its own `Ledger` with its own state model, engine, and validator set; the
  parent's per-block work is O(anchors), not O(child transactions).
- **Heterogeneous mixtures under one root of trust.** A UTXO/PoW child and an
  account/BFT child can hang off the same parent (composable chains already give
  each child its own identity via the genesis-committed blueprint).
- **Cross-chain verification.** Today a light client verifies one chain. With
  anchoring, a client that trusts the parent's validator set can verify claims
  about any child through a chain of existing proofs — no new cryptography.

What it is **not**: anchoring does not make a child more secure than its own
validator set. A child's finality is decided by the child's validators; the
parent *witnesses* it. §3 spells this out.

---

## 2. The two readings of "one block is a sub-blockchain"

### 2.1 Containment — the parent block carries the child's blocks
The parent block literally embeds the child's blocks (and re-executes them, or
at least hashes them). Rejected for v1:
- Unbounded block size; the parent's `max_block_bytes` becomes meaningless.
- The parent would have to run every child's state model and engine to validate
  — the exact coupling composable chains were built to avoid.
- No real system does this at scale; every production design (parachains, IBC,
  rollups) commits to a child *summary* and keeps child data on the child.

### 2.2 Anchoring — the parent block commits to the child's finalized tip ✅
The parent block carries **anchors**: `{child_chain_id, child_header,
child_certificate}`. Validation is self-contained (header + certificate + the
child's registered validator set), deterministic, and bounded. Child data
availability stays with the child's network; the parent proves *that* the child
finalized a given header, and the header's `state_root` lets anyone verify
*what* was in that state.

**Decision (proposed): anchoring.** An optional "carry the intermediate child
headers too" mode can be added later for data availability without changing the
commitment structure (§9 Q1).

---

## 3. Trust model (state this before touching code)

| Party | Trusts | Because |
|---|---|---|
| Parent validators | The child's registered validator set to finalize honestly | A valid `CommitCertificate` under that set is the child's own finality rule |
| Child validators | The parent to order and witness their anchors | The parent is BFT-finalized itself |
| A light client of the parent | The parent validator set | Existing `LightClient` rule |
| The same client, about a child | Parent finality **and** the child validator set the parent registered | The proof chain in §7 is only as strong as both |

Consequences:
- A **malicious child validator supermajority** can anchor a bogus state root.
  The parent cannot detect this (it does not execute the child). This is the
  same limit as any light client and must be documented, not hidden.
- A **child equivocation** (two certified headers at one height) is detectable
  by the parent: the registry only ever accepts a strictly increasing height, so
  the second one is rejected, and the pair is valid evidence (§6.4).
- A **malicious parent supermajority** can refuse to anchor (censorship) but
  cannot forge a child certificate. Children keep finalizing regardless; they
  only lose the parent's witnessing.

---

## 4. Data model

### 4.1 `Anchor` (new, `types.hpp`)
```
struct CertifiedHeader { BlockHeader header; Bytes certificate; };  // certificate = canonical CommitCertificate bytes
struct Anchor {
  std::uint64_t child_chain_id;
  std::vector<CertifiedHeader> headers;    // a CONSECUTIVE segment: child tip+1 … tip+k
  Bytes encode() const; static Anchor decode(CodecReader&); static Anchor decode_all(ByteView);
  Hash hash() const;                       // BLAKE3( kAnchor ‖ encode() )
};
```
**Why a segment, not a single header.** `LightClient::accept` verifies a header
only as the *next* one (height +1, parent hash, and the embedded last-commit for
the current tip). Reusing it unchanged means every child header must be seen —
so an anchor carries the consecutive run of child headers finalized since the
last anchor, each with its certificate, bounded by `max_headers_per_anchor`.
A child can therefore advance many blocks per parent block, and the parent
still verifies ancestor finality, not just the newest certificate.
The certificate is kept as opaque canonical bytes in `types.hpp` (decoded and
verified by the registry) so the core types stay free of the consensus layer.

Anchors inside one block are **canonically ordered**: ascending
`child_chain_id`, no duplicates (one anchor per child per parent block; §9 Q4).

### 4.2 Header version 3 (`BlockHeader`)
Exactly the pattern used for `receipt_root` (v2): **bump the version, never add
an unconditional field**, so every v1/v2 encoding and golden vector stays
byte-identical.
```
std::uint8_t version = 3;         // serialized only when version >= 3:
Hash anchor_root{};               // Merkle root over this block's anchors (kLeaf(anchor.encode()))
Hash subchain_root{};             // commitment to the POST-block subchain registry (§4.4)
```
Rule: a v3 header on a chain whose genesis has **no** subchain policy is
rejected (`check_header`). A v3 header with zero anchors is valid
(`anchor_root = zero_hash()`), so a parent chain can produce blocks with nothing
to anchor.

### 4.3 `Block`
```
std::vector<Anchor> anchors;      // serialized only when header.version >= 3
```

### 4.4 `SubchainRegistry` (new, owned by the `Ledger`, not the `StateMachine`)
The registry is deliberately **outside** the pluggable state machine: anchoring
is a ledger-level fact about *other chains*, independent of whether this
chain's own state is account- or UTXO-shaped.
```
struct ChildEntry {
  std::uint64_t chain_id;
  BlockHeader   genesis_header;     // the child's identity anchor (see note)
  consensus::ValidatorSet vset;     // the child's validator set (fixed in v1 — §8)
  std::uint8_t  child_max_depth;    // the child's own subtree depth (I10)
  BlockHeader   tip;                // last anchored child header (height 0 = none yet)
};
```
**Note (found while building):** registering the child by its genesis *hash*
is circular — the child's genesis hash commits its `ParentBinding` (the
parent's genesis hash), and the parent's genesis hash commits the
registration. The child's genesis *header* (chain id, genesis state root, and,
for a child that is itself a parent, its initial registry root) does not depend
on the binding, so it is the identity the parent commits to; the binding then
holds the parent's genesis hash without a cycle.
```
class SubchainRegistry {
  // Deterministic, pure: returns an error string or applies the anchor.
  std::string apply(const Anchor& a);                  // §6 rules, in order
  Hash root() const;                                   // §4.5
  Bytes serialize() const; static SubchainRegistry deserialize(ByteView); // snapshot/restore
};
```
Internally each `ChildEntry` drives a `LightClient` — `LightClient::accept
(header, cert)` **is** the anchor verification; we do not write a second one.

### 4.5 `subchain_root`
`BLAKE3( kSubchainRegistry ‖ count ‖ for each child ascending by chain_id:
chain_id ‖ genesis_header.hash() ‖ tip.hash() ‖ tip.height ‖ tip.state_root )`.
A flat hash is enough for v1 (the child count is policy-bounded and small); a
Merkle root can replace it under a version bump if per-child proofs must be
O(log n). Proof of a child's tip = the full sorted list (bounded by
`max_children`) — trivial and total.

### 4.6 Genesis commitment — `SubchainPolicy` (parent side)
Same pattern as `assurance_profile` / `membership_policy` / `chain_blueprint`:
a canonical byte blob appended **last** to `GenesisSpec` encoding
(`GenesisSpec.subchain_policy`, empty = this chain anchors nothing), committed
into the genesis hash, orthogonal to every other genesis field.
```
struct SubchainPolicy {
  std::uint16_t max_children;           // 0 = not a parent
  std::uint8_t  max_depth;              // this chain's allowed depth below it (1 = children only)
  std::uint16_t max_anchors_per_block;
  std::vector<ChildEntry-at-genesis> children;   // static registration (v1) — §9 Q2
  Bytes encode() const; static SubchainPolicy decode_all(ByteView); Hash hash() const;
  void commit_to(GenesisSpec&); static optional<SubchainPolicy> from_genesis(const GenesisSpec&);
};
```
Domain tags to add: `kAnchor`, `kSubchainRegistry`, `kSubchainPolicy` (new
values only — existing hashes untouched).

### 4.7 Child side — `parent_binding` (proposed, §9 Q5)
`GenesisSpec.parent_binding = parent_chain_id ‖ parent_genesis_hash` (empty =
root chain). Committed into the child's genesis hash so a child **cannot be
re-parented** and so a parent can check at registration that the child
declares *it* as parent (mutual binding). Costs nothing at runtime.

---

## 5. Invariants (each becomes a test in §10)

I1. **Version fence.** Encodings of v1/v2 headers and blocks are byte-identical
    to today; all golden vectors pass unchanged.
I2. **Policy fence.** Without a genesis `subchain_policy`, any v3 header, any
    block with anchors, is rejected. With one, `anchors.size() ≤
    max_anchors_per_block` and every `child_chain_id` is registered.
I3. **Canonical anchor order**, no duplicate child per block; anything else →
    block invalid (`anchor_root` alone would not catch order — we check both).
I4. **`anchor_root` and `subchain_root` recomputed and compared** by every
    validator, exactly like `tx_root` / `state_root` / `receipt_root`.
I5. **Monotone child height.** `anchor.child_header.height > entry.tip.height`,
    and `child_header.chain_id == entry.chain_id`.
I6. **Certificate validity** = `LightClient::accept` under the registered set
    (quorum, signer uniqueness, BLS aggregate over `child_header.hash()`).
I7. **All-or-nothing.** A block with one bad anchor changes nothing (registry is
    applied on a working copy, same as the state machine's clone-then-commit).
I8. **Determinism.** Registry application is a pure function of (registry,
    anchors); replaying genesis→H through `ForkChoice::ledger_at` yields the
    identical `subchain_root`. No clock, no randomness, no map-order dependence.
I9. **Snapshot fidelity.** `Ledger::snapshot()/restore()` round-trips the
    registry; `subchain_root` equal before and after.
I10. **Depth bound.** A child whose own policy allows depth `d` cannot be
    registered under a parent allowing depth `< d+1` (checked at genesis
    validation; the parent never verifies grandchildren — the child did).
I11. **Equivocation is inert.** Two certified child headers at one height: the
    first anchored wins, the second is rejected by I5; the block carrying it is
    invalid (fail-closed), and the pair is exportable evidence.
I12. **Cross-chain proof soundness.** `verify_subchain_proof` accepts iff the
    parent header commits (via `subchain_root`) to the claimed child tip AND
    `verify_state_proof(child_tip.state_root, proof)` holds. Any tampered link
    fails.

---

## 6. Validation order (extends `Ledger::process_block`)

```
2.   check_header (existing)  +  I2 version/policy fence
2a.  block-size ceilings (existing)  +  anchors.size() ≤ max_anchors_per_block
3.   tx_root recompute (existing)
3b.  anchor_root recompute  +  I3 canonical order          ← before any per-anchor crypto
3c.  registry.clone(); for each anchor: apply (I5, I6)     ← BLS verify here, bounded by policy
4.   per-tx pipeline on the state working copy (existing)
5.   state_root compare (existing)
5a.  receipt_root compare (existing)
5b.  subchain_root compare against registry-working-copy.root()
6.   commit: state, receipts, registry — together
```
Anchors are validated **before** transactions so a peer cannot make us execute a
whole block's transactions and only then fail on a cheap anchor check; the
cheap structural checks (3b) come before the BLS work (3c).

### 6.4 Evidence
`wire::kEvidence` already exists for validator double-signing. A
`SubchainEquivocation{child_chain_id, header_a, cert_a, header_b, cert_b}`
payload is a natural extension; **out of v1 scope** except for the test that
proves I11 (rejection), with the wire type reserved.

---

## 7. Cross-chain proofs (the payoff)

```
SubchainProof {
  BlockHeader   parent_header;      // caller verifies it via the parent LightClient / certificate
  registry_list (sorted ChildEntry summaries)   // recomputes subchain_root
  std::uint64_t child_chain_id;
  StateProof    child_state_proof;  // against the child tip's state_root
}
bool verify_subchain_proof(const Hash& parent_subchain_root, const SubchainProof&);
```
Verification: rebuild `subchain_root` from the list, compare; find the child;
`verify_state_proof(child.tip.state_root, child_state_proof)`. Recursion: a
grandchild proof is a `SubchainProof` whose "parent" is the child's header, with
the child's own registry list — the same function applied twice. No new crypto.

---

## 8. Known limits (state up front, do not paper over)

- **Child validator sets are fixed at registration** in v1, because
  `LightClient` has no validator-set-change rule yet (it is an open [H] item in
  `IMPROVEMENTS.md`). A child that rotates validators cannot be anchored until
  that lands — and it must land *in `LightClient`*, so anchoring inherits it.
- **Only certificate-bearing children** can be anchored in v1: bft / pos /
  vrf-pos, whose finality is a BLS `CommitCertificate`. PoW children have
  probabilistic finality (a confirmation-depth rule would be a different anchor
  kind); foreign engines have opaque justifications the parent can only check if
  the same foreign verifier is registered in the parent's process (possible —
  §9 Q3 — but not v1).
- **The parent does not execute children.** A child supermajority can anchor a
  false state root. Documented in §3.
- **Registration is static (genesis) in v1.** Adding a child later means a new
  parent chain identity — by design (§9 Q2 for the dynamic path).

---

## 9. Open questions — MUST be answered before implementing

| # | Question | Recommendation |
|---|---|---|
| Q1 | Anchoring (§2.2) or containment (§2.1)? | **Anchoring.** Containment breaks bounds and coupling. |
| Q2 | Child registration: static at parent genesis, or dynamic via a governance tx? | **Static in v1.** Dynamic registration needs a signed registration tx type, an authority rule (who may register?), and a registry that grows — all deferrable, none reversible once shipped. |
| Q3 | Which child engines may be anchored? | **Cert-bearing only (bft/pos/vrf-pos)** in v1. PoW (depth rule) and foreign (registered verifier) as follow-ups behind the policy. |
| Q4 | One anchor per child per parent block, or many? | **One anchor per child**, carrying a bounded consecutive *segment* of child headers (§4.1) — the registry verifies each in order. |
| Q5 | Should the child genesis bind to its parent (`parent_binding`, §4.7)? | **Yes, in v1** — it is a genesis-encoding change on the child, cheapest done now. |
| Q6 | Who moves anchors from child to parent? | **In-process `SubchainRelay`** for v1 (child `Chain` → parent `build_block(..., anchors)`); a networked relay is transport work, not consensus work, and can come later. |
| Q7 | Header layout: two roots (`anchor_root`, `subchain_root`) or fold the registry into the state machine? | **Two roots, registry in the Ledger.** Keeps the state model pluggable and the anchoring model-agnostic. |
| Q8 | Depth: allow a child to be a parent in v1 (3-level tree)? | **Yes** — it falls out of the design for free and the test proves recursion; bound it with `max_depth`. |

---

## 10. Test plan (must be green before the policy can be non-empty anywhere)

Unit / codec
- `Anchor`, `SubchainPolicy`, `SubchainRegistry` encode/decode round-trip;
  **codec totality fuzz** for each (`test_codec_fuzz` sweep).
- Golden vectors: all existing v1/v2 vectors unchanged (I1); a new v3 header +
  block vector pinned.

Ledger (`test_subchains.cpp`) — the validation matrix, one check per row
- no policy + v3 header → reject; no policy + anchors → reject (I2)
- unknown child; wrong `child_header.chain_id`; stale height (≤ tip); equal
  height; duplicate child in one block; wrong order; over `max_anchors_per_block`
  → all reject, ledger untouched (I3, I5, I7)
- bad certificate: not enough signers, duplicate signer, wrong block id, flipped
  aggregate byte → reject (I6)
- `anchor_root` / `subchain_root` mismatch with otherwise-valid anchors → reject (I4)
- happy path: anchor advances the child tip; `subchain_root` changes; a second
  block with a newer child tip advances again; a block with zero anchors keeps
  the root.
- equivocation: two certified child headers at one height → second rejected;
  block carrying it invalid (I11).
- snapshot/restore round-trip mid-tree (I9); `ForkChoice::ledger_at` replay
  equals the live root (I8).

Integration
- **Three-level tree**: root ← child ← grandchild, real `ChainAssembler`
  chains (bft child, pos grandchild), real certificates; the root's registry
  reflects the child's tip whose own registry reflects the grandchild's (I10).
- **Cross-chain proof**: prove a grandchild account balance from a root header
  (I12), then tamper each link (parent root, registry list, child tip, state
  proof) and confirm rejection.
- **Determinism across nodes**: 4-validator BFT parent over the TCP reactor
  (existing `run_tcp_bft_node`), a proposer includes anchors, all four commit
  the same block hash — proves anchors validate identically on non-proposers.
- Sanitizers: ASan/UBSan on all of the above, TSan on the reactor run.

Bindings (after the core is green): C ABI `blockchainer_subchain_policy_encode`,
`_genesis_commit_subchains`, `_anchor_encode`, `_chain_propose_anchored`,
`_chain_child_tip`, `_verify_subchain_proof`; Go/npm/JVM/Swift/Dart each build a
two-level tree and verify a cross-chain proof — 1-1 parity as always.

---

## 11. Staged delivery (each stage: build, full `ctest`, sanitizers, commit, push)

| Stage | Content | Behaviour change |
|---|---|---|
| 0 | This doc; answer §9 | none |
| 1 | `Anchor`, `SubchainPolicy`, `SubchainRegistry` types + codec + domain tags + genesis fields (parent policy, child `parent_binding`) + fuzz + golden vectors | none (v3 never produced) |
| 2 | `Ledger`: registry, v3 `check_header` fence, §6 validation, snapshot/restore; `build_block(txs, ts, anchors)`; validation matrix tests | parent chains can anchor |
| 3 | `SubchainProof` + `verify_subchain_proof`; three-level tree + cross-chain proof tests | proofs |
| 4 | `SubchainRelay` (in-process) + `AssembledChain`/`Chain` integration; TCP determinism test | end-to-end |
| 5 | C ABI + five bindings + smoke tests | parity |
| 6 | `IMPROVEMENTS.md`, `THREAT_MODEL.md` (§3 limits), memory | docs |

Stages 1–3 are pure C++ and fully testable without networking; nothing ships
enabled until stage 3's tests are green, because until then no genesis carries
a `subchain_policy`.

---

## 12. Recommendation

Build **anchoring** (not containment), **static registration**, **cert-bearing
children only**, **registry in the Ledger with two new v3 header roots**, with
the **child bound to its parent at genesis**. This reuses four verified pieces
unchanged — the version-gated header pattern, `LightClient::accept`, the
genesis-commitment pattern, and `verify_state_proof` — so the new
consensus-critical surface is small: the anchor ordering/monotonicity rules and
the registry commitment. Those are exactly the invariants §10 pins down first.
