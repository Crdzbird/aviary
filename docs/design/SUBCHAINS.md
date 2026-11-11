# Subchains - a block that anchors a whole blockchain - Design Doc

Status: **v1 BUILT (all six stages, 2026-09-19). Version 2 (dynamic registration, Merkle registry, closing) is designed in Part 2 below, sections 13 to 26.** Recommendations approved as proposed; two findings changed the design while building - registration by genesis header (section 4.4 note) and BFT-only children (section 8). This document exists because the feature
is delicate: it changes what a block commits to, adds a second trust level, and
touches consensus-critical code. Every question in section 9 must be answered before a
line of implementation is written; every invariant in section 5 must have a test in
section 10 before the feature is enabled on any chain.

Related: `ARCHITECTURE.md` section 4 (canonical encoding), section 6 (block pipeline), section 10
(light client); `design/CREDENTIAL_GATE.md` (the genesis-commitment pattern
this reuses); `IMPROVEMENTS.md` (composable chains, foreign components).

---

## 0. One-paragraph summary

A **subchain** is an ordinary aviary chain (any mixture the
`ChainBlueprint` allows) whose finalized progress is **anchored** into a
*parent* chain: a parent block carries, for each child it anchors, the child's
finalized block header plus the child's commit certificate. The parent's
validators verify the certificate against the child's registered validator set
 - exactly what a `LightClient` already does - and record the child's new tip in
a registry committed into the parent header. The result is a tree of chains:
each level orders, timestamps, and vouches for the *finality* of the level
below, and anyone holding a parent header can prove a child's state
(`parent.subchain_root -> child.state_root -> account`). "One block is a
sub-blockchain" is realised as **one block commits to a sub-blockchain's
finalized state**; the parent never re-executes the child.

---

## 1. Motivation - where it fits

- **Horizontal scale without breaking the determinism mandate.** Each child runs
  its own `Ledger` with its own state model, engine, and validator set; the
  parent's per-block work is O(anchors), not O(child transactions).
- **Heterogeneous mixtures under one root of trust.** A UTXO/PoW child and an
  account/BFT child can hang off the same parent (composable chains already give
  each child its own identity via the genesis-committed blueprint).
- **Cross-chain verification.** Today a light client verifies one chain. With
  anchoring, a client that trusts the parent's validator set can verify claims
  about any child through a chain of existing proofs - no new cryptography.

What it is **not**: anchoring does not make a child more secure than its own
validator set. A child's finality is decided by the child's validators; the
parent *witnesses* it. section 3 spells this out.

---

## 2. The two readings of "one block is a sub-blockchain"

### 2.1 Containment - the parent block carries the child's blocks
The parent block literally embeds the child's blocks (and re-executes them, or
at least hashes them). Rejected for v1:
- Unbounded block size; the parent's `max_block_bytes` becomes meaningless.
- The parent would have to run every child's state model and engine to validate
 - the exact coupling composable chains were built to avoid.
- No real system does this at scale; every production design (parachains, IBC,
  rollups) commits to a child *summary* and keeps child data on the child.

### 2.2 Anchoring - the parent block commits to the child's finalized tip [x]
The parent block carries **anchors**: `{child_chain_id, child_header,
child_certificate}`. Validation is self-contained (header + certificate + the
child's registered validator set), deterministic, and bounded. Child data
availability stays with the child's network; the parent proves *that* the child
finalized a given header, and the header's `state_root` lets anyone verify
*what* was in that state.

**Decision (proposed): anchoring.** An optional "carry the intermediate child
headers too" mode can be added later for data availability without changing the
commitment structure (section 9 Q1).

---

## 3. Trust model (state this before touching code)

| Party | Trusts | Because |
|---|---|---|
| Parent validators | The child's registered validator set to finalize honestly | A valid `CommitCertificate` under that set is the child's own finality rule |
| Child validators | The parent to order and witness their anchors | The parent is BFT-finalized itself |
| A light client of the parent | The parent validator set | Existing `LightClient` rule |
| The same client, about a child | Parent finality **and** the child validator set the parent registered | The proof chain in section 7 is only as strong as both |

Consequences:
- A **malicious child validator supermajority** can anchor a bogus state root.
  The parent cannot detect this (it does not execute the child). This is the
  same limit as any light client and must be documented, not hidden.
- A **child equivocation** (two certified headers at one height) is detectable
  by the parent: the registry only ever accepts a strictly increasing height, so
  the second one is rejected, and the pair is valid evidence (section 6.4).
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
  std::vector<CertifiedHeader> headers;    // a CONSECUTIVE segment: child tip+1 ... tip+k
  Bytes encode() const; static Anchor decode(CodecReader&); static Anchor decode_all(ByteView);
  Hash hash() const;                       // BLAKE3( kAnchor || encode() )
};
```
**Why a segment, not a single header.** `LightClient::accept` verifies a header
only as the *next* one (height +1, parent hash, and the embedded last-commit for
the current tip). Reusing it unchanged means every child header must be seen - 
so an anchor carries the consecutive run of child headers finalized since the
last anchor, each with its certificate, bounded by `max_headers_per_anchor`.
A child can therefore advance many blocks per parent block, and the parent
still verifies ancestor finality, not just the newest certificate.
The certificate is kept as opaque canonical bytes in `types.hpp` (decoded and
verified by the registry) so the core types stay free of the consensus layer.

Anchors inside one block are **canonically ordered**: ascending
`child_chain_id`, no duplicates (one anchor per child per parent block; section 9 Q4).

### 4.2 Header version 3 (`BlockHeader`)
Exactly the pattern used for `receipt_root` (v2): **bump the version, never add
an unconditional field**, so every v1/v2 encoding and golden vector stays
byte-identical.
```
std::uint8_t version = 3;         // serialized only when version >= 3:
Hash anchor_root{};               // Merkle root over this block's anchors (kLeaf(anchor.encode()))
Hash subchain_root{};             // commitment to the POST-block subchain registry (section 4.4)
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
  consensus::ValidatorSet vset;     // the child's validator set (fixed in v1 - section 8)
  std::uint8_t  child_max_depth;    // the child's own subtree depth (I10)
  BlockHeader   tip;                // last anchored child header (height 0 = none yet)
};
```
**Note (found while building):** registering the child by its genesis *hash*
is circular - the child's genesis hash commits its `ParentBinding` (the
parent's genesis hash), and the parent's genesis hash commits the
registration. The child's genesis *header* (chain id, genesis state root, and,
for a child that is itself a parent, its initial registry root) does not depend
on the binding, so it is the identity the parent commits to; the binding then
holds the parent's genesis hash without a cycle.
```
class SubchainRegistry {
  // Deterministic, pure: returns an error string or applies the anchor.
  std::string apply(const Anchor& a);                  // section 6 rules, in order
  Hash root() const;                                   // section 4.5
  Bytes serialize() const; static SubchainRegistry deserialize(ByteView); // snapshot/restore
};
```
Internally each `ChildEntry` drives a `LightClient` - `LightClient::accept
(header, cert)` **is** the anchor verification; we do not write a second one.

### 4.5 `subchain_root`
`BLAKE3( kSubchainRegistry || count || for each child ascending by chain_id:
chain_id || genesis_header.hash() || tip.hash() || tip.height || tip.state_root
|| vset.hash() || tip_vset.hash() )` - the child's validator set in force next and
the set that certified the tip (added with validator rotation, so a parent
header attests the child's current set and a snapshot cannot smuggle one).
A flat hash is enough for v1 (the child count is policy-bounded and small); a
Merkle root can replace it under a version bump if per-child proofs must be
O(log n). Proof of a child's tip = the full sorted list (bounded by
`max_children`) - trivial and total.

### 4.6 Genesis commitment - `SubchainPolicy` (parent side)
Same pattern as `assurance_profile` / `membership_policy` / `chain_blueprint`:
a canonical byte blob appended **last** to `GenesisSpec` encoding
(`GenesisSpec.subchain_policy`, empty = this chain anchors nothing), committed
into the genesis hash, orthogonal to every other genesis field.
```
struct SubchainPolicy {
  std::uint16_t max_children;           // 0 = not a parent
  std::uint8_t  max_depth;              // this chain's allowed depth below it (1 = children only)
  std::uint16_t max_anchors_per_block;
  std::vector<ChildEntry-at-genesis> children;   // static registration (v1) - section 9 Q2
  Bytes encode() const; static SubchainPolicy decode_all(ByteView); Hash hash() const;
  void commit_to(GenesisSpec&); static optional<SubchainPolicy> from_genesis(const GenesisSpec&);
};
```
Domain tags to add: `kAnchor`, `kSubchainRegistry`, `kSubchainPolicy` (new
values only - existing hashes untouched).

### 4.7 Child side - `parent_binding` (proposed, section 9 Q5)
`GenesisSpec.parent_binding = parent_chain_id || parent_genesis_hash` (empty =
root chain). Committed into the child's genesis hash so a child **cannot be
re-parented** and so a parent can check at registration that the child
declares *it* as parent (mutual binding). Costs nothing at runtime.

---

## 5. Invariants (each becomes a test in section 10)

I1. **Version fence.** Encodings of v1/v2 headers and blocks are byte-identical
    to today; all golden vectors pass unchanged.
I2. **Policy fence.** Without a genesis `subchain_policy`, any v3 header, any
    block with anchors, is rejected. With one, `anchors.size() <=
    max_anchors_per_block` and every `child_chain_id` is registered.
I3. **Canonical anchor order**, no duplicate child per block; anything else ->
    block invalid (`anchor_root` alone would not catch order - we check both).
I4. **`anchor_root` and `subchain_root` recomputed and compared** by every
    validator, exactly like `tx_root` / `state_root` / `receipt_root`.
I5. **Monotone child height.** `anchor.child_header.height > entry.tip.height`,
    and `child_header.chain_id == entry.chain_id`.
I6. **Certificate validity** = `LightClient::accept` under the registered set
    (quorum, signer uniqueness, BLS aggregate over `child_header.hash()`).
I7. **All-or-nothing.** A block with one bad anchor changes nothing (registry is
    applied on a working copy, same as the state machine's clone-then-commit).
I8. **Determinism.** Registry application is a pure function of (registry,
    anchors); replaying genesis->H through `ForkChoice::ledger_at` yields the
    identical `subchain_root`. No clock, no randomness, no map-order dependence.
I9. **Snapshot fidelity.** `Ledger::snapshot()/restore()` round-trips the
    registry; `subchain_root` equal before and after.
I10. **Depth bound.** A child whose own policy allows depth `d` cannot be
    registered under a parent allowing depth `< d+1` (checked at genesis
    validation; the parent never verifies grandchildren - the child did).
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
2a.  block-size ceilings (existing)  +  anchors.size() <= max_anchors_per_block
3.   tx_root recompute (existing)
3b.  anchor_root recompute  +  I3 canonical order          <- before any per-anchor crypto
3c.  registry.clone(); for each anchor: apply (I5, I6)     <- BLS verify here, bounded by policy
4.   per-tx pipeline on the state working copy (existing)
5.   state_root compare (existing)
5a.  receipt_root compare (existing)
5b.  subchain_root compare against registry-working-copy.root()
6.   commit: state, receipts, registry - together
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
the child's own registry list - the same function applied twice. No new crypto.

---

## 8. Known limits (state up front, do not paper over)

- ~~Child validator sets are fixed at registration~~ - **lifted** (validator
  rotation, 2026-09-19): it landed in `LightClient`, so anchoring inherited it.
  The registration is the child's *genesis* set; the registry tracks the set in
  force next and the set that certified the tip, and `subchain_root` commits
  both hashes (section 4.5).
- **Only BFT children can be anchored in v1.** Correction found while building:
  in this codebase the PoS and VRF-PoS engines justify a block with a single
  *leader signature* and carry `PosData`/`VrfPosData` in `consensus_data`, not
  a quorum `CommitCertificate` + `ConsensusData{vset_hash, last_commit}` - so
  the light-client rule (`LightClient::accept`) cannot verify them. They need a
  certificate-producing finality gadget before they can be children. PoW
  children have probabilistic finality (a confirmation-depth rule would be a
  different anchor kind); foreign engines have opaque justifications the parent
  can only check if the same foreign verifier is registered in its process.
  Any engine can be the PARENT (every built-in engine implements
  `propose_anchored`).
- **The parent does not execute children.** A child supermajority can anchor a
  false state root. Documented in section 3.
- **Registration is static (genesis) in v1.** Adding a child later means a new
  parent chain identity - by design (section 9 Q2 for the dynamic path).
  Version 2 (Part 2) registers children in any block under a dynamic policy;
  a version 1 policy keeps this limit.

---

## 9. Open questions - MUST be answered before implementing

| # | Question | Recommendation |
|---|---|---|
| Q1 | Anchoring (section 2.2) or containment (section 2.1)? | **Anchoring.** Containment breaks bounds and coupling. |
| Q2 | Child registration: static at parent genesis, or dynamic via a governance tx? | **Static in v1.** Dynamic registration needs a signed registration tx type, an authority rule (who may register?), and a registry that grows - all deferrable, none reversible once shipped. **Answered for version 2 in Part 2** (sections 13 to 26): a signed registry operation in a version 4 block, registrar keys committed at genesis, a Merkle registry. |
| Q3 | Which child engines may be anchored? | **BFT only** in v1 (see section 8 - PoS/VRF here are leader-signed, not certificate-finalized). PoW (depth rule), PoS/VRF (finality gadget) and foreign (registered verifier) as follow-ups behind the policy. |
| Q4 | One anchor per child per parent block, or many? | **One anchor per child**, carrying a bounded consecutive *segment* of child headers (section 4.1) - the registry verifies each in order. |
| Q5 | Should the child genesis bind to its parent (`parent_binding`, section 4.7)? | **Yes, in v1** - it is a genesis-encoding change on the child, cheapest done now. |
| Q6 | Who moves anchors from child to parent? | **In-process `SubchainRelay`** for v1 (child `Chain` -> parent `build_block(..., anchors)`); a networked relay is transport work, not consensus work, and can come later. |
| Q7 | Header layout: two roots (`anchor_root`, `subchain_root`) or fold the registry into the state machine? | **Two roots, registry in the Ledger.** Keeps the state model pluggable and the anchoring model-agnostic. |
| Q8 | Depth: allow a child to be a parent in v1 (3-level tree)? | **Yes** - it falls out of the design for free and the test proves recursion; bound it with `max_depth`. |

---

## 10. Test plan (must be green before the policy can be non-empty anywhere)

Unit / codec
- `Anchor`, `SubchainPolicy`, `SubchainRegistry` encode/decode round-trip;
  **codec totality fuzz** for each (`test_codec_fuzz` sweep).
- Golden vectors: all existing v1/v2 vectors unchanged (I1); a new v3 header +
  block vector pinned.

Ledger (`test_subchains.cpp`) - the validation matrix, one check per row
- no policy + v3 header -> reject; no policy + anchors -> reject (I2)
- unknown child; wrong `child_header.chain_id`; stale height (<= tip); equal
  height; duplicate child in one block; wrong order; over `max_anchors_per_block`
  -> all reject, ledger untouched (I3, I5, I7)
- bad certificate: not enough signers, duplicate signer, wrong block id, flipped
  aggregate byte -> reject (I6)
- `anchor_root` / `subchain_root` mismatch with otherwise-valid anchors -> reject (I4)
- happy path: anchor advances the child tip; `subchain_root` changes; a second
  block with a newer child tip advances again; a block with zero anchors keeps
  the root.
- equivocation: two certified child headers at one height -> second rejected;
  block carrying it invalid (I11).
- snapshot/restore round-trip mid-tree (I9); `ForkChoice::ledger_at` replay
  equals the live root (I8).

Integration
- **Three-level tree**: root <- child <- grandchild, real `ChainAssembler`
  chains (bft child, pos grandchild), real certificates; the root's registry
  reflects the child's tip whose own registry reflects the grandchild's (I10).
- **Cross-chain proof**: prove a grandchild account balance from a root header
  (I12), then tamper each link (parent root, registry list, child tip, state
  proof) and confirm rejection.
- **Determinism across nodes**: 4-validator BFT parent over the TCP reactor
  (existing `run_tcp_bft_node`), a proposer includes anchors, all four commit
  the same block hash - proves anchors validate identically on non-proposers.
- Sanitizers: ASan/UBSan on all of the above, TSan on the reactor run.

Bindings (after the core is green): C ABI `aviary_subchain_policy_encode`,
`_genesis_commit_subchains`, `_anchor_encode`, `_chain_propose_anchored`,
`_chain_child_tip`, `_verify_subchain_proof`; Go/npm/JVM/Swift/Dart each build a
two-level tree and verify a cross-chain proof - 1-1 parity as always.

---

## 11. Staged delivery (each stage: build, full `ctest`, sanitizers, commit, push)

| Stage | Content | Behaviour change |
|---|---|---|
| 0 | This doc; answer section 9 | none |
| 1 | `Anchor`, `SubchainPolicy`, `SubchainRegistry` types + codec + domain tags + genesis fields (parent policy, child `parent_binding`) + fuzz + golden vectors | none (v3 never produced) |
| 2 | `Ledger`: registry, v3 `check_header` fence, section 6 validation, snapshot/restore; `build_block(txs, ts, anchors)`; validation matrix tests | parent chains can anchor |
| 3 | `SubchainProof` + `verify_subchain_proof`; three-level tree + cross-chain proof tests | proofs |
| 4 | `SubchainRelay` (in-process) + `AssembledChain`/`Chain` integration; TCP determinism test | end-to-end |
| 5 | C ABI + five bindings + smoke tests | parity |
| 6 | `IMPROVEMENTS.md`, `THREAT_MODEL.md` (section 3 limits), memory | docs |

Stages 1-3 are pure C++ and fully testable without networking; nothing ships
enabled until stage 3's tests are green, because until then no genesis carries
a `subchain_policy`.

---

## 12. Recommendation

Build **anchoring** (not containment), **static registration**, **cert-bearing
children only**, **registry in the Ledger with two new v3 header roots**, with
the **child bound to its parent at genesis**. This reuses four verified pieces
unchanged - the version-gated header pattern, `LightClient::accept`, the
genesis-commitment pattern, and `verify_state_proof` - so the new
consensus-critical surface is small: the anchor ordering/monotonicity rules and
the registry commitment. Those are exactly the invariants section 10 pins down first.

---

# Part 2 - Version 2: dynamic registration, a Merkle registry, closing

Status: **DESIGNED 2026-11-11, built on the branch `claude/dynamic-subchains-c93c8b`
to the recommendations below, NOT released.** Every rule in this part sits
behind a new genesis policy format and header version 4, which no existing
chain uses, so each decision in section 24 can still be changed before a
release at the cost of regenerating only the new vectors. Part 1 (sections 0
to 12) stays exactly as built: a chain whose genesis carries a version 1
policy behaves and encodes as in aviary 1.0.0.

This part answers Q2 (section 9). It exists because of a concrete consumer: a
system where every user owns one chain, users sign up over time in the tens
of thousands, every user chain is committed into one main chain, and the
server restarts. Version 1 cannot serve it for three reasons, each of which
this part removes:

| Limit in version 1 | Where | Version 2 |
|---|---|---|
| Children exist only in the parent genesis | section 4.6, section 8 | registered by a signed operation in any parent block (section 15) |
| `subchain_root` is one flat hash, a proof carries every child | section 4.5, section 7 | Merkle registry, a proof carries one record and about log2(n) hashes (sections 16, 17) |
| `max_children` is 16 bit | section 4.6 | 32 bit record index, policy bound on open children (section 18) |

Sections 13 to 26 use the same vocabulary as Part 1. "Parent validators" are
the validators of the chain that anchors; "the registry" is the parent's
ledger level view of its children.

---

## 13. Summary of the mechanism

A parent chain whose genesis commits a **dynamic subchain policy** produces
version 4 blocks. A version 4 block carries, besides transactions and anchors,
a list of **registry operations**. An operation registers a child, closes a
child, or replaces the registrar keys. Each operation is authorised by
signatures from **registrars**, keys the parent genesis commits to, and every
parent validator verifies those signatures and the operation's rules while
validating the block, exactly as it verifies a transaction signature or an
anchor. Operations are applied in block order, then anchors, on a working copy
of the registry; the header commits the operations (`ops_root`) and the
registry after them (`subchain_root`).

The registry commitment becomes a binary Merkle tree with one leaf per child
ever registered, in registration order. A leaf never moves, so registering a
child appends one leaf and anchoring a child rewrites one leaf: both cost
O(log n) hashes. A proof for one child is that child's record plus its Merkle
path.

The parent identity never changes. The genesis hash commits the policy
(limits, the first registrar set, and any children registered at genesis);
everything that happens later is in blocks.

---

## 14. Dynamic policy (parent genesis)

```
struct Registrar { std::uint8_t sig_algo; Bytes public_key; };

struct DynamicSubchainPolicy {
  std::uint8_t  max_depth;               // levels allowed below this chain, >= 1 (I10)
  std::uint32_t max_children;            // bound on OPEN children, 0 = no policy bound
  std::uint16_t max_anchors_per_block;   // 0 = unlimited
  std::uint16_t max_headers_per_anchor;  // 0 = unlimited
  std::uint16_t max_ops_per_block;       // 0 = unlimited
  std::uint8_t  registrar_threshold;     // k of the n registrars must approve an operation
  std::vector<Registrar> registrars;     // index significant, unique, at most 64
  std::vector<ChildRegistration> children;  // registered at genesis, ascending unique chain_id
};
```

It is carried in the same genesis field as the version 1 policy
(`GenesisSpec.subchain_policy`), so `GenesisSpec` does not change by one byte
and `aviary_genesis_commit_subchain_policy` serves both formats. The two
formats are told apart by a prefix (section 20.1) that can never begin a valid
version 1 policy.

Registrars use the transaction signature schemes that already exist
(`SigAlgo` 1 Ed25519, 2 hybrid Ed25519 plus ML-DSA-65, 4 SLH-DSA), so no new
cryptography enters the core and a fortress posture parent can require a post
quantum registrar. `kUtxoWitness` (3) is not a key scheme and is refused.

Validity (`DynamicSubchainPolicy::validate(own_chain_id)`, checked at boot,
an invalid policy is a boot failure):

- `max_depth >= 1`.
- At most 64 registrars; each `sig_algo` is 1, 2 or 4; an Ed25519 or SLH-DSA
  key is exactly 32 bytes, a hybrid key is non empty and at most 4096 bytes;
  no two registrars share the same (scheme, key).
- With n registrars: n = 0 requires threshold 0; n >= 1 requires
  1 <= threshold <= n.
- Genesis children obey the version 1 rules (ascending unique ids, not the
  parent's own id, non empty validators with non zero power, genesis header at
  height 0 with the matching chain id, `child_max_depth + 1 <= max_depth`) and
  number at most `max_children` when that is non zero.

**A policy with no registrars is a sealed registry**: no operation can ever be
authorised. That is the version 1 behaviour (static children) with the
version 2 commitment and proofs, and it is the migration path for a new
static family that wants small proofs.

---

## 15. Registry operations

### 15.1 Structure

```
struct Approval { std::uint16_t registrar_index; Bytes signature; };

struct SubchainOp {
  std::uint8_t  kind;             // 1 register, 2 close, 3 set registrars
  std::uint32_t authority_epoch;  // the registry's current epoch, see 15.4
  std::uint64_t valid_until;      // last parent height at which it may be applied, 0 = no expiry
  Bytes         payload;          // by kind, below
  std::vector<Approval> approvals;  // ascending unique registrar_index
};
```

Payload by kind:

- **1 register**: the canonical `ChildRegistration` of Part 1, unchanged: the
  child's chain id, its genesis header, its genesis validator set and its
  subtree depth.
- **2 close**: `chain_id`, `final_height`, `final_tip_hash`.
- **3 set registrars**: a new `registrar_threshold` and registrar list.

### 15.2 What a registrar signs

A registrar signs the 32 byte **authorisation digest**

```
H( 0x13 || u64 parent_chain_id || parent_genesis_hash[32] || u8 kind ||
   u32 authority_epoch || u64 valid_until || bytes(payload) )
```

with its own scheme. Signing a tagged digest, not raw fields, follows the
peer record convention already in the core, and it keeps these signatures
apart from transaction signatures: a transaction's signing bytes are never 32
bytes long (the shortest verifiable one is 49), so a registrar key that is
also an account key cannot be tricked across the two uses.

The digest binds the parent by chain id **and genesis hash**. A ledger
therefore needs to know its own genesis hash: `ChainSpec` gains
`genesis_hash`, filled by `GenesisSpec::chain_spec()`, and a ledger with a
dynamic policy refuses to boot when it is all zero. Nothing else reads the
field, so chains without a dynamic policy are unaffected.

### 15.3 Validation, per operation, at parent height `h`

Performed by every validator inside `Ledger::run_block`, in this order, on
the registry working copy `R`. The first failure invalidates the block.

1. `kind` is 1, 2 or 3.
2. `authority_epoch == R.epoch`.
3. `valid_until == 0` or `h <= valid_until`.
4. `R` has at least one registrar; the approvals are ascending and unique by
   index, every index is below the registrar count, and there are at least
   `R.threshold` of them.
5. The payload decodes strictly (trailing bytes rejected) and passes the rule
   for its kind:
   - **register**: the id is not the parent's own; the id has never been
     registered on this parent (an open or a closed record blocks it); the
     validator set is well formed (`ValidatorSet` construction, non zero
     powers); the genesis header is at height 0 with the same chain id;
     `child_max_depth + 1 <= max_depth`; the open count is below
     `max_children` when that is non zero; the record count is below
     2^32 - 1.
   - **close**: the child is registered and open, and `final_height` and
     `final_tip_hash` equal its recorded tip.
   - **set registrars**: the new set passes the policy rules of section 14,
     and the epoch is below 2^32 - 1.
6. Every approval's signature verifies over the authorisation digest under
   that registrar's key and scheme. All approvals are checked, not only the
   first `threshold`: an operation carrying one bad signature is invalid.

Cheap structural checks (1 to 5) run before any signature work (6).

### 15.4 Effect

- **register**: append a record at index `R.count`: status open,
  `registered_height = h`, tip = the genesis header, both validator sets = the
  registered set.
- **close**: status closed, `closed_height = h`. The record stays forever with
  its final tip, so the child's last anchored state remains provable. A closed
  child accepts no further anchor and its chain id can never be registered
  again on this parent. Closing is final; there is no reopen.
- **set registrars**: replace the registrar set and threshold, and increment
  `R.epoch`. Because every operation names the epoch it was signed for, a
  rotation also revokes every operation signed earlier and not yet included,
  and an old rotation cannot be replayed after rotating back to the same
  keys. Setting an empty list seals the registry permanently.

### 15.5 Order inside a block

Operations in the order the block lists them, each seeing the effects of the
ones before it; then the anchors. Consequences, all deliberate:

- A child registered in a block may be anchored in the same block.
- A close names the tip recorded by earlier blocks, so the final anchor of a
  child lands in an earlier block than its close. A block cannot both advance
  and close one child, which keeps "which tip is final" unambiguous for the
  registrar that signed it.
- An operation that follows a registrar change in the same block must carry
  the new epoch and the new set's signatures.

Anchors keep the Part 1 rules (canonical order, one per child, segment bound,
the light client rule per header) with one addition: the child must be
**open**.

---

## 16. Registry commitment (header version 4)

### 16.1 The record

One record per child ever registered. It is the Merkle leaf preimage and the
unit a proof carries:

```
struct ChildRecord {
  std::uint32_t index;              // registration order, equals the leaf position
  std::uint64_t chain_id;
  std::uint8_t  status;             // 1 open, 2 closed
  std::uint8_t  child_max_depth;
  Hash          genesis_header_hash;
  std::uint64_t registered_height;  // parent height of the registering block, 0 = parent genesis
  std::uint64_t closed_height;      // 0 while open
  std::uint64_t tip_height;
  Hash          tip_hash;
  Hash          tip_state_root;
  Hash          tip_subchain_root;  // the child's own registry root, zero unless it is a parent
  Hash          vset_hash;          // the child's set in force for its next header
  Hash          tip_vset_hash;      // the set that certified the tip
};
```

The record repeats the three fields of the tip header a verifier needs
(height, state root, registry root) so a proof needs no child header. They
are derived by the parent validators from the header the light client rule
accepted, so they are exactly as trustworthy as `tip_hash`.

### 16.2 The root

```
leaf_i         = H( 0x10 || canonical(ChildRecord_i) )
children_root  = merkle::compute_root( leaf_0 .. leaf_{n-1} )
authority_hash = H( 0x14 || u8 threshold || list(u8 sig_algo || bytes(public_key)) )
subchain_root  = H( 0x11 || u32 record_count || u32 open_count ||
                    u32 authority_epoch || authority_hash[32] || children_root[32] )
```

`merkle::compute_root` is the existing tree (odd node carried up, node hash
`H(0x01 || left || right)`, a fixed empty root for zero leaves), unchanged.
The outer hash exists because the existing proof shape lets the prover state
the leaf count: binding the count into `subchain_root` makes it authenticated,
so a record's position cannot be reinterpreted. It also commits the registrar
set and epoch, so a parent header attests who may currently change the
registry, and a snapshot cannot smuggle a different registrar set (the same
argument Part 1 makes for the child validator set hashes).

Leaves are in **registration order and never move**. Genesis children take
indices 0.. in ascending chain id order; each later registration takes the
next index. Uniqueness of a chain id across records is enforced by the
validators (section 15.3 rule 5) and is not provable from the root alone; see
decision D4 for the alternative that would make it provable.

### 16.3 Cost

| Event | Hashes |
|---|---|
| register one child | 1 leaf + about log2(n) nodes |
| anchor one child (any segment length) | 1 leaf + about log2(n) nodes |
| close one child | 1 leaf + about log2(n) nodes |
| block that touches k children | at most k (1 + log2(n)) |

The in memory registry keeps the tree levels and applies a block as a small
overlay on an immutable, shared base, so validating a block copies nothing
proportional to n. The overlay is folded into the base when the block
commits and dropped when it does not (I7, all or nothing).

---

## 17. Proofs

### 17.1 One child against one parent header

```
struct ChildProof {
  ChildRecord   record;
  std::uint32_t record_count;
  std::uint32_t open_count;
  std::uint32_t authority_epoch;
  Hash          authority_hash;
  std::vector<Hash> siblings;   // bottom up, the existing merkle::Proof order
};
```

`verify_child_proof(subchain_root, proof)` returns the record or nothing:

1. `record.index < record_count`, `status` is 1 or 2.
2. Recompute the leaf, then `children_root` from the siblings under the shape
   implied by `(record.index, record_count)`. The sibling count must be
   exactly what that shape needs, one more or one fewer fails.
3. Recompute `subchain_root` from the five outer fields and compare.

The caller obtains `subchain_root` from a parent header it has verified
(a parent `LightClient`, or a certificate), exactly as in Part 1. The verifier
then **reads** the record: it compares `tip_hash` or `tip_state_root` with
what it expects, and it sees `status`, so it can tell an open child from a
closed one. Size: a 230 byte record, 44 bytes of outer fields, and 32 bytes
per tree level: 17 levels for 100 000 children, 20 for a million.

### 17.2 A path to a state entry, any depth, mixed formats

```
struct SubchainProofV2 {
  struct Hop { std::uint8_t kind; Bytes body; };   // 1: a Part 1 hop, 2: a ChildProof
  std::vector<Hop> hops;
  StateProof state_proof;
};
bool verify_subchain_proof_v2(const Hash& parent_subchain_root, const SubchainProofV2&);
```

Start with the parent root. A kind 1 hop is verified as in section 7 (the
flat list recomputes the current root) and yields the child's tip header; a
kind 2 hop is a `ChildProof` verified as above and yields a record. Either
way the next root is that child's own `subchain_root` and the running state
root is its tip state root. After the last hop the state proof must hold
under the running state root. A tree may therefore mix version 3 and version
4 parents level by level; the two root formats carry different domain tags,
so a hop of the wrong kind cannot verify. The Part 1 `SubchainProof` and
`verify_subchain_proof` are unchanged.

A proof that a specific child BLOCK is covered: prove the child (17.1), then
show the block is an ancestor of `tip_hash` by the child's own header chain,
or prove the entry in the child's state. Proving that an operation was
included in a given parent block is possible against `ops_root` (a standard
Merkle inclusion) and is not built in this version.

---

## 18. How many children

- A record index is 32 bit, so one parent holds at most 2^32 - 1 records,
  open and closed together, for its whole life.
- `max_children` (32 bit) bounds the OPEN children. 0 means no policy bound.
- Per block work is bounded by `max_ops_per_block`,
  `max_anchors_per_block` and `max_headers_per_anchor`, not by n.
- Memory on a parent validator: an open child keeps its tip header and its
  validator sets (shared when equal), a closed child drops the sets. In
  practice a few hundred bytes per child plus 64 bytes of tree, so 100 000
  open children is tens of megabytes.

---

## 19. Header and block version 4

```
BlockHeader v4 = v3 layout || ops_root[32]           (still before consensus_data)
Block       v4 = bytes(header) || list(bytes(tx)) || list(bytes(anchor)) || list(bytes(op))
ops_root       = merkle root over H( 0x12 || canonical(op) ) in block order, zero hash for none
```

`anchor_root` keeps its Part 1 definition. `subchain_root` in a version 4
header is the section 16 root; in a version 3 header it is the Part 1 flat
hash. The domain tags differ (0x0D and 0x11), so the two can never be
confused.

### 19.1 Why versions 1, 2 and 3 stay byte for byte valid

Encoding is by version gate, the pattern used for versions 2 and 3: `ops_root`
is written and read only when `version >= 4`, and a block's operation list
only when its header is version 4 or later. For versions 1 to 3 the encoder
and decoder execute exactly the statements they executed in 1.0.0, so every
existing encoding, hash and golden vector is unchanged. The existing vectors
(`vectors/block_vectors.txt`, `hash_blake3.txt`, `proof_vectors.txt`) are
replayed unchanged by the suite.

### 19.2 The version fence

| Genesis policy | Header version accepted |
|---|---|
| none | below 3 (unchanged) |
| version 1 policy | exactly 3 |
| dynamic policy | exactly 4 |

**One compatibility note the owner must know (decision D7).** Aviary 1.0.0
accepts ANY version of 3 or more on a version 1 parent chain and encodes it
with the version 3 layout. No honest 1.0.0 node produces such a header (it
always writes 3), but a Byzantine proposer on a multi validator version 1
parent could, and 1.0.0 validators would certify it. Once version 4 has a
layout, that header no longer decodes the same way, so a node built from this
branch refuses it. Any future header version has this property; it cannot be
avoided, only chosen: this design fails closed (exactly 3). Operators of a
multi validator version 1 parent should upgrade its validators together.
Single operator chains and every chain without a subchain policy are not
affected.

### 19.3 Which genesis header a dynamic parent has

Version 4, `anchor_root` and `ops_root` zero, `subchain_root` the section 16
root over the genesis children (an empty tree for none). A child's
registration still names its genesis header, whatever its version.

---

## 20. Canonical encodings (in the style of docs/CODEC_SPEC.md)

All integers big endian; `bytes(x)` is a u32 length then the bytes;
`list(T)` is a u32 count then each element; fixed hashes carry no prefix.
Every top level decode rejects trailing bytes.

### 20.1 Layouts

- `DynamicSubchainPolicy` = `00 00 00 || u8 format(=2) || u8 max_depth ||
  u32 max_children || u16 max_anchors_per_block || u16 max_headers_per_anchor ||
  u16 max_ops_per_block || u8 registrar_threshold || list(Registrar) ||
  list(bytes(ChildRegistration))`.
  The three zero bytes are the format escape. Read as a version 1 policy they
  are `max_children = 0, max_depth = 0`, which `SubchainPolicy::validate`
  rejects (`max_depth` must be at least 1), so no valid version 1 policy
  begins with them and no existing chain can be reinterpreted. `format` other
  than 2 is rejected.
- `Registrar` = `u8 sig_algo || bytes(public_key)`.
- `SubchainOp` = `u8 kind || u32 authority_epoch || u64 valid_until ||
  bytes(payload) || list(u16 registrar_index || bytes(signature))`.
- register payload = `ChildRegistration` (Part 1 layout).
- close payload = `u64 chain_id || u64 final_height || final_tip_hash[32]`.
- set registrars payload = `u8 registrar_threshold || list(Registrar)`.
- `ChildRecord` = `u32 index || u64 chain_id || u8 status || u8 child_max_depth ||
  genesis_header_hash[32] || u64 registered_height || u64 closed_height ||
  u64 tip_height || tip_hash[32] || tip_state_root[32] || tip_subchain_root[32] ||
  vset_hash[32] || tip_vset_hash[32]` (230 bytes, fixed).
- `ChildProof` = `bytes(ChildRecord) || u32 record_count || u32 open_count ||
  u32 authority_epoch || authority_hash[32] || list(sibling[32])`.
- `SubchainProofV2` = `list(u8 kind || bytes(hop)) || bytes(StateProof)`.
- `BlockHeader` and `Block` version 4: section 19.
- Registry snapshot section: section 22.

Non canonical forms rejected on decode, beyond the generic codec rules:
approvals not ascending or not unique; `status` outside {1, 2}; a `closed_height`
that is non zero on an open record or zero on a closed one where the context
can tell (registry restore); an unknown hop kind; an unknown operation kind is
decodable (so a block carrying one can be hashed and refused with a reason)
but never valid.

### 20.2 Domain tags

New values only; none collides with 0x00 to 0x0F.

| Tag | Meaning |
|-----|---------|
| `0x10` | Registry leaf: `H(0x10 || canonical(ChildRecord))` |
| `0x11` | Dynamic registry commitment (`subchain_root` of a version 4 header) |
| `0x12` | Registry operation leaf under `ops_root` |
| `0x13` | Registry operation authorisation digest (what a registrar signs) |
| `0x14` | Registrar set commitment (`authority_hash`) |

The dynamic policy hash reuses `0x0E`; the escape prefix keeps its preimage
disjoint from every version 1 policy preimage.

---

## 21. Determinism, path by path

Each new validation path is a pure function of committed bytes. None reads a
clock, randomness, a map's iteration order, a float, or anything outside the
block and the parent's own state.

| Path | Inputs | Why every node agrees |
|---|---|---|
| Policy decode and validation | genesis bytes | strict decode; fixed checks; fails at boot identically everywhere |
| Operation decode | block bytes | strict decode, trailing bytes rejected |
| Operation validity (15.3) | the operation, the working registry, the block height | fixed check order; the registry is a function of genesis plus earlier blocks; height comes from the header already checked sequential |
| Signature checks | digest, registrar key, signature | deterministic verification in the core (the same functions transactions use); the digest is canonical bytes |
| Operation effect (15.4) | as above | appends at `count`, or rewrites one named record; no choice is left to the node |
| Anchors | Part 1 rule on the post operation registry | unchanged `LightClient::accept` |
| `ops_root` | operation encodings in block order | existing Merkle construction |
| `subchain_root` | records in index order, counters, registrar set | index order is registration order, fixed by block order; the in memory lookup map is used only to find a record, never iterated for hashing |
| Overlay and fold | implementation detail | the root of base plus overlay equals `compute_root` over the full leaf list; a differential test checks it against the naive computation after random registrations, anchors and closes |
| Snapshot restore | snapshot bytes | strict decode, then the recomputed root must equal the tip header's `subchain_root` |

Arithmetic: counters are checked before increment (record count and epoch
below 2^32 - 1); heights are the codec's u64 below 2^63.

New invariants (each has a test):

I13. **Version 4 fence** as in the table of 19.2.
I14. **Registration is authorised**: an operation without `threshold` valid
     registrar signatures over the right digest never changes the registry.
I15. **Parent bound**: an operation signed for one parent (id, genesis hash)
     is invalid on any other.
I16. **Epoch bound**: an operation is valid only in the epoch it names.
I17. **Unique forever**: a chain id is registered at most once per parent.
I18. **Closed is final**: no anchor, no reopen, record and proof remain.
I19. **Incremental root equals the naive root.**
I20. **Proof soundness**: `verify_child_proof` accepts iff the record is the
     leaf at its index under the committed count; any changed field, sibling,
     count or outer field fails.
I21. **Restart fidelity**: snapshot, destroy, restore, continue produces the
     same bytes as an uninterrupted run (section 22).

---

## 22. Persistence

A restart must reproduce the registry exactly, and the ledger snapshot is
where it lives. `Ledger::snapshot()` of a dynamic parent appends one section
(a chain with no policy or a version 1 policy writes exactly what it wrote in
1.0.0):

```
registry section = u32 authority_epoch || u8 registrar_threshold || list(Registrar) ||
                   list( u64 chain_id || u8 status || u8 child_max_depth ||
                         genesis_header_hash[32] || u64 registered_height || u64 closed_height ||
                         bytes(tip_header) || bytes(vset) || bytes(tip_vset) )
```

Records are in index order (the index is the position). `vset` and `tip_vset`
are the canonical `ValidatorSet` encodings for an open child and empty for a
closed one. Restore decodes strictly, refuses duplicate chain ids and
inconsistent status fields, rebuilds the tree, and requires the recomputed
root to equal the restored tip header's `subchain_root`.

What a whole chain needs beyond the ledger (the chain snapshot of the C ABI,
format in docs/CODEC_SPEC.md):

- the genesis hash it belongs to (a snapshot is refused under another genesis);
- the ledger snapshot (tip, state, registry) and the receipts of the tip block;
- the engine's consensus state. For BFT: the validator set in force, the set
  that certified the tip, the tip's commit certificate (it is embedded in the
  next proposal), a scheduled rotation, and the recorded equivocators. The
  other built in engines keep nothing beyond the ledger;
- optionally the archive of finalized blocks with their certificates.

Not captured, by design: validator secret keys (the caller supplies them
again on restore; a snapshot holds no secret), transactions queued for the
next proposal, buffered events and metrics.

---

## 23. Threat model delta

| Threat | Analysis | Mitigation |
|---|---|---|
| **A hostile registrant**: someone asks for, or a registrar signs, a child that is malicious | The parent commits only what the registration states: a genesis header and a validator set. It never executes the child, so a malicious child harms only those who trust that child's validators (the Part 1 limit). A registrant cannot touch another child's record. | Registration needs `threshold` registrar signatures; the id must be new; depth, set shape and genesis header are checked. A verifier that holds the child's genesis recomputes its header, compares `genesis_header_hash`, and checks the child's `ParentBinding` names this parent. |
| **A compromised registrar key** (below threshold: nothing; at threshold: register and close at will) | The worst a registrar quorum can do is register junk children up to the bounds, close children (the parent stops witnessing them; the children themselves keep running and their recorded tips stay provable), or replace the registrars. It cannot forge a child tip: only a child certificate moves a tip. | Threshold k of n; post quantum registrar keys available; rotation by the current quorum bumps the epoch and voids outstanding signatures. If an attacker rotates first, recovery is social (a new parent), the same as losing a validator supermajority. |
| **Registration spam** | Each record is permanent (a leaf, a lookup entry). | Only registrars can create records; `max_ops_per_block` bounds per block work and growth; `max_children` bounds open children; a closed record costs a hash and a map entry, not validator sets. An open, fee priced registration market is deliberately not offered. |
| **Proposer includes an invalid operation** | Same class as an invalid anchor. | `validate_block` dry run before every vote; structural checks before signature work; a bad operation invalidates the whole block and changes nothing. Work per proposal is bounded by `max_ops_per_block` times the registrar count. |
| **Child supermajority anchors a false root** | Unchanged from Part 1 and still undetectable by the parent. Dynamic registration makes it easier to create children, so more of them will exist with small validator sets (one key per user chain). | Documented, not mitigated. A consumer whose children have a single validator must treat an anchor as a timestamped, ordered witness of what that validator signed, not as independent validation. |
| **Replay of a registration across parents** | Two networks may share a chain id and even a registrar key (staging and production). | The digest binds the parent's genesis hash. A ledger with a dynamic policy refuses to run without it. |
| **Replay on the same parent** | A register is consumed by the id becoming taken; a close by the status change; a rotation by the epoch change. | Rules 2 and 5 of 15.3. |
| **A signed operation held back and included late** | An authorisation is public once gossiped. | `valid_until`; and any rotation voids all outstanding operations. |
| **Signature reuse across protocols** | A registrar key reused as an account or peer key. | Distinct domain tag and a 32 byte digest that no other signed message shape matches (15.2). |
| **Forged or reinterpreted child proof** | The prover supplies the leaf count in the existing proof shape. | The count, the open count, the epoch and the registrar hash are inside `subchain_root`; the index is inside the leaf; the sibling count must match the shape exactly. |
| **Tampered snapshot** | A restored registry with a swapped tip, validator set or registrar set. | The recomputed root must equal the tip header's `subchain_root`; the snapshot is bound to the genesis hash; decoding is strict. A snapshot remains local trusted storage: it is not authenticated against an attacker who can also rewrite the tip header inside it. Operators who need that keep the tip's certificate (archived) and verify it. |
| **Version confusion** | A version 3 proof or header presented where version 4 is expected, or the reverse. | Distinct domain tags per root format; exact version fence per policy format. |

---

## 24. Decisions for the owner to confirm

Each is implemented as recommended. None affects any chain that exists today
except the note under D7.

| # | Decision | Recommendation | Alternative |
|---|---|---|---|
| D1 | How a registration reaches the chain | A block section of signed operations with its own header root (the pattern anchors use). | A transaction type: rejected, a transaction body belongs to the pluggable state machine and the registry is deliberately outside it (Q7). |
| D2 | Who may register | Registrar keys committed in the parent genesis, k of n, verified by every validator. An empty list seals the registry. | Proposer discretion with validators voting only for operations they scheduled (the validator rotation pattern): smaller surface, but on a leader signed or proof of work parent any leader could register, and the rule is not checkable by a replaying node. |
| D3 | Registrar rotation and the authority epoch | Built: the current quorum installs a new set, the epoch voids outstanding signatures. | Fixed registrars for the life of the chain: simpler, but a compromised or lost key then means a new parent chain. |
| D4 | Registry commitment | Append only Merkle tree in registration order, count bound into the root, reusing the existing tree and proof code. | A sparse Merkle tree keyed by chain id: also proves that an id is NOT registered and makes id uniqueness provable from the root, at the price of a new 64 level tree and a compressed proof format in consensus code. Choose it only if absence proofs become a requirement; it would be another version bump. |
| D5 | Bounds | 32 bit record index; `max_children` bounds open children; 0 = unbounded. | Bounding total records instead, which would let closes run a parent out of capacity. |
| D6 | Closing | A registrar signed close naming the final tip; permanent; the id is never reusable; the record stays provable. | Child initiated close (a closing header certified by the child): a useful addition, not needed by a server run registry, left for a later version. |
| D7 | Header version and fence | Version 4 with `ops_root`; fence is exact per policy format. Note the 1.0.0 laxity described in 19.2. | None that keeps a context free decoder. |
| D8 | Operation expiry | `valid_until` parent height, 0 = none. | No expiry: 8 bytes smaller, stale authorisations then live until a rotation. |
| D9 | Parent identity in the digest | Chain id and genesis hash, via a new `ChainSpec.genesis_hash`. | Policy hash only: cheaper, but two networks with the same policy bytes could replay each other. |
| D10 | Policy format marker | The `00 00 00 02` prefix inside the existing genesis field. | A new trailing genesis field: would make `GenesisSpec` decoding accept an optional tail. |
| D11 | What the parent learns about a child at registration | Its genesis header and genesis validator set (the Part 1 `ChildRegistration`), not its full genesis. | Carrying the child's whole genesis so the parent checks the binding itself: couples the parent to the child's state model, against section 2.2. |

---

## 25. Known limits of version 2

- **The chain tree stays static.** `ChainTree` builds a declared family with
  version 1 policies. A dynamic parent is driven through the chain API. Tree
  snapshots are built (Part 1 families restart too).
- **BFT children only**, as in Part 1.
- **No absence proofs** (D4) and **no operation inclusion proofs** (17.2).
- **No child initiated close** (D6).
- **The parent does not check the child's `ParentBinding`** (D11).
- **Networked proposers**: `RoundDriver` and `NetworkedNode` take anchors for
  their next proposal; they do not yet take operations. A networked parent
  validates and applies operation carrying blocks, it cannot yet propose them
  from the network API. The in process engines can.
- **Bindings**: the C ABI, Go and Dart expose version 2. The npm, JVM, Swift
  and Flutter packages do not until their next release.

---

## 26. Test plan for version 2

Unit (`test_dynamic_subchains.cpp`): codec round trips and strict rejections
for every structure in section 20; policy validation matrix; the operation
validation matrix of 15.3, one check per rule (wrong epoch, expired, too few
approvals, unordered approvals, index out of range, bad signature, wrong
parent genesis hash, wrong parent id, duplicate id, re registering a closed
id, bad depth, over `max_children`, close with a stale tip, close twice,
anchor to a closed child, operations on a sealed registry); rotation and the
epoch; I13 to I20.

Merkle (`test_merkle.cpp`): the appendable tree against `compute_root` and
`make_proof` over random appends and updates.

Scale: register several thousand children after genesis, anchor them, prove
one, and assert the proof grows with log2(n), not n.

Restart (`test_chain_snapshot.cpp`, and through the C ABI): build, snapshot,
destroy, restore, continue; compare every later block, root and proof with an
uninterrupted run (I21); every snapshot rejection.

Fuzz: the new decoders in `fuzz_codec` and the `test_codec_fuzz` sweep; chain
snapshot restore in `fuzz_restore`.

Vectors: `vectors/subchain_v2_vectors.txt`, replayed by the C++ suite, Go and
Dart; the existing three vector files unchanged.
