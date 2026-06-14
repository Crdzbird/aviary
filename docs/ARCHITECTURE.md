# Blockchainer — Architecture & Design

> A **C++** distributed-ledger core (built on **cryptolib** for all crypto), exposed
> through a **C ABI** with **Dart and Go bindings**, built to construct custom
> chains, run them privately or permissionlessly, connect to other chains, and
> stream/observe/verify everything along the way.
>
> Structure mirrors cryptolib: one C++ implementation → one C ABI → thin language
> packages. Consensus logic lives **once**, in C++; bindings only marshal.
>
> Status: **DESIGN — not yet implemented.** This document is the contract we review
> before writing code. Every later module must trace back to a decision here.

---

## 0. What we are building

Blockchainer is a **C++ library/toolkit**, not a single chain. A consumer assembles
a chain from parts:

- a **state machine** (account *or* UTXO — chosen at genesis),
- a **consensus engine** (BFT reference; PoS/PoW pluggable),
- a **crypto suite** (classical, post-quantum, or hybrid — all from cryptolib),
- a **network/transport** profile (private/permissioned or permissionless),

and gets a node that produces, validates, finalizes, gossips, streams, and exposes
blocks — plus clients that verify and bridge to *other* chains.

The core is **C++** and is consumed from **Dart and Go** (and any future language)
through a single **C ABI**, exactly as cryptolib is. The bindings are thin: they
marshal structured data in and out, and never reproduce consensus-critical logic.
A wallet UI in Flutter, a relayer in Go, and a full node all drive the *same* C++
engine.

### Goals (from the brief, made concrete)

| Brief word        | Concrete requirement |
|-------------------|----------------------|
| "dynamic"         | Pluggable state machine, consensus, crypto, storage, transport — selected by config at genesis, not by forking the code. |
| "custom chains"   | A `ChainSpec` (genesis) fully parameterizes a chain; two chains differ only by spec, not by code. |
| "connect to other blockchains" | Outbound RPC clients (EVM / Solana / Bitcoin) **and** inbound light-client verification + bridge/relay primitives. |
| "observability"   | Structured logs, metrics (counter/gauge/histogram), tracing spans, and a typed event bus across consensus/networking/state. |
| "streaming"       | Streaming block/state sync over encrypted channels, plus a live event stream API for consumers. |
| "verification"    | Full-node validation, light-client/SPV header+proof verification, and a standalone verification API. |
| "as safe as possible" | Safety-over-liveness consensus, post-quantum-capable signatures (hybrid), committing AEAD channels, strict determinism, adversarial test discipline. |
| "used privately"  | Permissioned mode: known validator set, encrypted+authenticated transport (Noise XX), no public discovery required. |
| *(multi-language)* | C++ core + C ABI + **Dart and Go** bindings (extensible to more), mirroring cryptolib. One engine, many callers. |

### Non-goals (explicitly out, for now)

- A production smart-contract VM. The state machine is pluggable; a VM is a *future*
  state-machine implementation, not part of the core.
- Token economics / a launched mainnet. We build the engine, not a coin.
- Re-implementing any cryptographic primitive. **All** crypto comes from cryptolib.

---

## 1. Trust & threat model (frame before design)

We support a **spectrum**, configured per chain:

| Mode | Validator set | Adversary bound | Transport | Finality |
|------|---------------|-----------------|-----------|----------|
| **Private / permissioned** (default reference) | Known, fixed or governance-rotated | Byzantine `f < n/3` | Authenticated + encrypted (Noise XX), optional allow-list | Deterministic, instant (BFT) |
| **Consortium** | Permissioned membership, larger set | `f < n/3` + Sybil-resistant membership | Encrypted gossip | Deterministic (BFT) |
| **Permissionless** | Open, stake- or work-weighted | 51% / long-range / nothing-at-stake | Public gossip + discovery | Probabilistic (PoS/PoW plug-in) |

**Assumed adversary (always):** can reorder, drop, delay, and replay network
messages; can run Sybil/eclipse attempts; can submit malformed and adversarial
transactions; can attempt double-spend; and is a **harvest-now-decrypt-later**
adversary against signatures over long-lived value (→ hybrid PQ signatures are a
first-class, per-chain option, not an afterthought).

**Safety invariant (the one we never break):** no two honest nodes finalize
conflicting state. When safety and liveness conflict, the chain **halts** rather
than forks. A halted chain recovers; a forked chain loses funds.

Full threat enumeration and economic-layer analysis live in [`THREAT_MODEL.md`](THREAT_MODEL.md)
(to be written in the security milestone, composing with cryptolib's threat model).

---

## 2. The determinism mandate

Every node must derive **byte-identical** state from identical inputs, or the chain
splits. This is the constraint that overrides convenience everywhere.

1. **Canonical serialization only.** One fixed byte encoding for every hashed object
   (block header, transaction, state node). Rules in §4. Hashing a non-canonical
   encoding is a consensus bug, not a style issue.
2. **No ambient nondeterminism in validation.** No wall-clock, no RNG, no map
   iteration order, no locale, no floating point in any code path that affects the
   hash or the accept/reject decision. Time and randomness enter only through
   protocol-defined, bounded fields (block timestamp ranges, VRF outputs).
3. **Checked / big-integer arithmetic.** Balances and supply use checked or
   arbitrary-precision integers. Silent overflow is a fork. No `int` wraparound on
   value math, ever.
4. **One implementation, in C++.** Consensus-critical logic (codec, hashing,
   Merkle, validation, fork choice) exists exactly once — in the C++ core. Dart and
   Go bindings marshal data in and out; they do **not** recompute consensus bytes.
   This removes the classic FFI hazard of a reference impl drifting from native:
   there is no second implementation to drift.
5. **Architecture-stable C++.** The one implementation must still be identical
   across architectures: watch endianness (we pin big-endian on the wire), struct
   padding (never hash a struct's raw memory — only the canonical codec output),
   and floating point (forbidden in consensus paths; no `-ffast-math` anywhere near
   them).

**Golden vectors are the cross-language determinism guard.** A fixed-input →
fixed-bytes/fixed-hash suite is the source of truth, and it is replayed **identically
through the C++ core and through every binding** (Dart, Go). If a Go caller and the
C++ core disagree on one vector, CI fails. This is how we prove the bindings stay
thin and the engine stays deterministic everywhere.

---

## 3. Cryptographic foundation (delegated to cryptolib)

We **do not implement crypto.** The C++ core links cryptolib's **prebuilt C ABI**
(`cryptolib_c` — link `libcryptolib_c` + include `bridge/cryptolib_c.h`). A thin
internal layer (`blockchainer::crypto`) wraps those C functions in
domain-separated, intent-named C++ operations (`hash_leaf`, `hash_node`,
`hash_header`, `sign_tx`, `verify_tx`, `aggregate_votes`…) returning real C++ types.

> **Linking choice (decision):** link the **self-contained `cryptolib_c` C ABI**,
> not cryptolib's header-only C++ API. This decouples blockchainer's build from
> cryptolib's transitive native-dep chain (libsodium / liboqs / blst / OpenSSL /
> BLAKE3) — blockchainer needs only the prebuilt `libcryptolib_c` artifact + one
> header. The tradeoff is a C-typed surface (lengths + out-pointers + buffer-free
> calls) that `blockchainer::crypto` immediately wraps in RAII C++ types, so the C
> shape never leaks past the seam. cryptolib's C ABI already guarantees exception
> isolation, indistinguishable error returns, and bounds-checked length math.

### Suite pins (defaults; per-chain configurable in `ChainSpec`)

| Role | Default primitive | cryptolib surface | Notes |
|------|-------------------|-------------------|-------|
| Block / object hash | **BLAKE3** | `cryptolib_blake3` (C ABI) | Fast tree hash; domain-separated via keyed/prefixed tags. **Gap:** not yet in the Dart binding — see §3.1. |
| Interop / EVM-compat hash | SHA-256 | `sha256` (Dart binding ✓) | For chains that must match foreign hashing. |
| Transaction signature | **Ed25519** | `ed25519Keygen/Sign/Verify` (✓) | Default. |
| Tx signature (long-lived value) | **Hybrid Ed25519 + ML-DSA-65** | `hybridSig*` (✓) | Secure if *either* scheme survives. Per-chain opt-in. |
| Pure PQ signature option | ML-DSA / SLH-DSA | `mlDsa*` / `slhDsa*` (✓) | For PQ-only chains. |
| Consensus votes | **BLS12-381 + aggregation** | `blsSign/Verify` (✓), `bls_aggregate(_verify)` (C ABI) | Aggregation collapses N votes → 1 sig. **Gap:** aggregate not in Dart binding — see §3.1. |
| P2P channel | **Noise XX** + hybrid KEM | `hybridKem*` (✓), Noise (C ABI) | Mutual auth + forward secrecy, PQ-hybrid key exchange. |
| Streaming sync encryption | SecretStream / committing AEAD | `streamEnc*/streamDec*` (✓), `committing*` (✓) | Committing AEAD closes partitioning-oracle classes. |
| Wallet / key custody | **Vault + Keyring** | `vault*`, `keyring*` (✓) | Envelope encryption, device + passphrase slots, rotation, anti-downgrade. |
| Validator key splitting | Shamir M-of-N | `shamir` (✓) | Optional: split a validator key across operators. |

### 3.1 cryptolib surface used by the core

BLAKE3 (`cryptolib_blake3`), Ed25519 (`cryptolib_ed25519_*`), ML-DSA/SLH-DSA, hybrid
signatures, BLS sign/verify, **BLS aggregate/aggregate-verify**
(`cryptolib_bls_aggregate(_verify)`), hybrid KEM, Noise XX, SecretStream
(`cryptolib_stream_*`), Vault, Keyring, and Shamir are all present in the
`cryptolib_c` C ABI, so the core calls them directly — nothing to add on the core
path.

> Separately, the cryptolib **Dart** binding was extended with `blake3` (+ `_keyed`,
> `_derive_key`) and `blsAggregate`/`blsAggregateVerify`. Those are **not** on
> blockchainer's core path (the core is C++), but remain useful for any client-side
> Dart crypto (e.g. a wallet signing locally) and for cryptolib itself. Note that
> determinism still requires consensus bytes to originate in the C++ core, so a
> binding never canonicalizes-and-hashes on its own.

### 3.2 The crypto seam (C++)

`blockchainer::crypto` exposes narrow C++ interfaces — `Hasher`, `Signer`,
`Verifier`, `Aggregator`, `SecureChannel` — with cryptolib-backed implementations.
This is **not** an abstraction tax to swap crypto libraries; it exists to (a) keep
domain separation centralized, (b) let `ChainSpec` select a suite by name
(Ed25519 vs hybrid vs PQ-only), and (c) make Merkle/validation logic testable
against a deterministic fake `Hasher` without pulling the whole native-dep chain
into every unit test. Consensus and ledger code depend on the seam, never on
cryptolib headers directly.

---

## 4. Canonical serialization & hashing

### Codec

A custom, explicitly-specified **canonical binary codec** (`blockchainer_codec`).
SSZ/RLP-inspired but minimal and our own, so the spec is ours and fully pinned:

- **Integers:** fixed-width, **big-endian**, unsigned unless stated. Value/amount
  fields use a 128-bit (or arbitrary-precision) representation with a canonical
  minimal-or-fixed encoding — pinned in the codec spec, never platform `int`.
- **Variable bytes / lists:** length-prefixed with a fixed-width count; no
  terminators.
- **Structs:** fields serialized in **declared order** (a fixed schema), never by
  reflection or map order.
- **Maps/sets:** forbidden in hashed objects unless represented as a list **sorted
  by canonical key bytes**; duplicate keys are an error.
- **No floats. No optionals-by-absence ambiguity** (an absent optional has one
  canonical encoding). **No nested re-encoding ambiguity.**
- Every decode is **bounds-checked** and **round-trips**: `decode(encode(x)) == x`
  and `encode(decode(b)) == b` for canonical `b`. Non-canonical input is rejected,
  not normalized (normalization would let two encodings share a hash → malleability).

The codec is the **most fuzzed** component in the system (§9).

### Hashing & domain separation

All hashing goes through `blockchainer_crypto` with a **domain tag** so a leaf can
never collide with an internal node or a header:

```
H_leaf(x)   = BLAKE3( 0x00 ‖ canonical(x) )
H_node(l,r) = BLAKE3( 0x01 ‖ l ‖ r )
H_header(h) = BLAKE3( 0x02 ‖ canonical(h) )
H_txid(tx)  = BLAKE3( 0x03 ‖ canonical(tx) )
```

(Tag bytes are pinned in the codec spec.) Domain separation prevents
second-preimage and cross-structure confusion attacks on the Merkle tree.

### Merkle commitments

- **Transaction root:** binary Merkle tree over `H_leaf(tx)` for each tx in block
  order. Odd-node handling pinned (duplicate-last vs explicit — we pin
  *no duplicate-last*; carry the lone node up — to avoid the Bitcoin CVE-2012-2459
  malleability class). Proofs are `(index, siblings[])`.
- **State root:** the state machine commits its state via a Merkleized structure
  (§5). The block header carries the post-state root so light clients can verify
  account/UTXO inclusion against a header.
- **Receipt/result root (optional):** for chains that expose execution receipts.
- **Future:** Verkle commitments as a state-root option for smaller proofs.

---

## 5. State machine (pluggable: account *or* UTXO)

The chain is parameterized by a `StateMachine` interface. The block-processing
pipeline (§6) is identical for both; only the implementation differs.

```cpp
class StateMachine {
 public:
  virtual ~StateMachine() = default;

  // Deterministic, canonical commitment to current state.
  virtual Hash state_root() const = 0;

  // Stateless + stateful checks; pure, no I/O, no clock.
  virtual ValidationResult validate(const Transaction&, const StateView&) const = 0;

  // Apply a validated tx to a working copy; returns events + fee outcome.
  virtual ApplyResult apply(const Transaction&, MutableStateView&) = 0;

  // Produce an inclusion proof for a key against state_root().
  virtual StateProof prove(const StateKey&) const = 0;
};
```

### Account model (default for general-purpose / private chains)

- State = map `address → AccountState{ balance: BigInt, nonce, codeHash?, storageRoot? }`.
- Replay protection via **monotonic nonce** per account.
- Committed as a **Merkleized key/value trie** keyed by `H(address)`, keys sorted
  canonically → deterministic root.
- Best fit for rich app logic, cross-chain state proofs, and permissioned ledgers.

### UTXO model

- State = set of unspent outputs; a tx consumes inputs (by outpoint) and creates
  outputs.
- Replay protection is intrinsic (an outpoint is spendable once).
- Committed as a Merkle tree / accumulator over the canonical UTXO set.
- Best fit for parallel validation and value-transfer privacy.

Both implement the same interface, so consensus, networking, storage, and the
light client are written **once**. `ChainSpec.stateModel` picks one at genesis.

---

## 6. Block & transaction model + processing pipeline

### Structures (canonical-encoded)

```
BlockHeader {
  version, chainId,
  height, parentHash,
  timestamp,                 // bounded, validated against parent + drift window
  txRoot, stateRoot, receiptRoot?,
  proposer / validatorBitmap,
  consensusData (opaque to ledger, defined by the engine)
}
Block      { header, transactions[] }
Transaction{ version, chainId, nonce/inputs, payload, fee, signature(s) }
```

`chainId` is bound into every signature and hash → **cross-chain replay protection**
by construction.

### Block processing (identical for all state models & engines)

```
1. Decode (canonical, bounds-checked)         → reject malformed
2. Header checks: parent linkage, height, timestamp window, version
3. txRoot recompute & compare                 → reject on mismatch
4. For each tx (in order): signature verify → StateMachine.validate → apply
5. Recompute stateRoot; compare to header     → reject on mismatch
6. Engine-specific consensus verification (§7)
7. Commit atomically (state + block + indexes) or roll back entirely
```

Steps 3–5 are the **determinism core** and the prime native-acceleration targets
(§8), always shadowed by a Dart reference for differential testing.

---

## 7. Consensus (pluggable engine; BFT reference)

```cpp
class ConsensusEngine {
 public:
  virtual ~ConsensusEngine() = default;

  virtual void start(NodeContext& ctx) = 0;
  // Drives proposal/voting; emits finalized blocks via the callback/stream.
  virtual void on_finalized(std::function<void(const FinalizedBlock&)>) = 0;
  // Validates the engine-specific consensus_data in a header.
  virtual bool verify_consensus(const BlockHeader&, const ConsensusState&) const = 0;
  virtual const ForkChoice& fork_choice() const = 0;
};
```

### Reference: BFT (Tendermint-style)

- **Round-based** propose → pre-vote → pre-commit → commit over a known validator
  set with voting power.
- **Deterministic instant finality:** a committed block is final — **no reorgs**.
  Fork choice is trivial (the committed chain); safety is the priority.
- **Safety:** `f < n/3` Byzantine tolerance. Locking + the pre-commit rule prevent
  two conflicting blocks at the same height. **On lost liveness the chain halts**,
  it does not fork.
- **Signatures:** validator votes are **BLS12-381**, **aggregated** into a single
  commit certificate (`bls_aggregate` — §3.1) → O(1)-ish certificate verification
  and compact headers.
- **Validator-set changes** via governance/staking txs take effect at a pinned
  height with an overlap/anti-downgrade rule.

### Pluggable alternatives (later milestones)

- **PoS** (stake-weighted, probabilistic finality, fork-choice rule, slashing).
- **PoW** (work-weighted, heaviest-chain fork choice).

Both reuse §6 verbatim; only `verifyConsensus`, `forkChoice`, and leader/validator
selection differ. The header's `consensusData` is opaque to the ledger, owned by the
engine — so the ledger never needs to know which engine is running.

---

## 8. Performance & the binding boundary

The entire core **is** native C++, so there is no "move this hot path to native"
question for our own logic — hashing, signature verification, Merkle computation,
validation, and fork choice already run at native speed, and cryptolib's primitives
are constant-time. Performance work happens *within* C++ and is **profile-driven**:
batch signature verification, Merkle roots over large tx sets, and streaming-sync
verification pipelines are the likely optimization targets, tuned only when a
benchmark proves the need.

The boundary that matters is now the **C ABI**, crossed by the Dart/Go bindings:

- **Coarse-grained calls.** Bindings invoke whole operations (build+sign a tx,
  validate a block, query state, verify a proof) — not per-field primitives — so
  marshaling cost is amortized and the hot loop stays inside C++.
- **No consensus work above the ABI.** A binding never serializes-canonically or
  hashes on its own; it hands structured fields to C++ and gets bytes/decisions
  back. This is both a determinism rule (§2) and a performance one.
- **Streaming across the ABI.** Long-running flows (sync, the event bus, §9/§12)
  use callback/stream patterns over the C ABI rather than buffering whole epochs.

`cpp-crypto-engineer`'s C-ABI boundary contract (exception isolation, bounds-checked
length math, no error oracles) governs the bridge surface.

---

## 9. Networking, mempool & streaming sync

`blockchainer_p2p`:

- **Transport:** authenticated, encrypted channels via **Noise XX** + **hybrid KEM**
  (cryptolib). Private mode adds a validator allow-list; permissionless mode adds
  discovery.
- **Gossip:** block and transaction propagation with deduplication and flood
  control; eclipse-resistance via diverse peer selection.
- **Mempool:** validated pending txs with per-account nonce ordering (account model)
  or outpoint conflict detection (UTXO), fee-priority eviction, replacement rules,
  and hard DoS limits (size caps, rate limits, per-peer accounting).
- **Sync:**
  - *Fast/streaming sync* — stream headers, then state snapshots + blocks over
    cryptolib **SecretStream/streaming AEAD**, verifying incrementally so a joining
    node keeps up with the network without buffering whole epochs.
  - *Light/SPV sync* — headers + Merkle proofs only (§10).

Streaming sync is the throughput path that would justify native acceleration (§8) if
profiling demands it.

---

## 10. Verification & light client

`blockchainer_client`:

- **Full verification:** the §6 pipeline, used by full nodes and by an offline
  "verify this block/chain segment" API.
- **Light client / SPV:** follow the header chain, verify each header's consensus
  certificate (§7), and check inclusion via Merkle proofs against `txRoot` /
  `stateRoot` — **without** holding full state.
- **Standalone verification API:** a consumer (wallet, dashboard, bridge relayer)
  can verify a proof or a header certificate in isolation. This is also the
  foundation for inbound interop (§11).

---

## 11. Interoperability ("connect to other blockchains")

Two directions:

**Outbound — talk to existing chains** (`blockchainer_interop`, per the skill's
web3-interaction reference): JSON-RPC clients for **EVM** (eth JSON-RPC, ABI
encode/decode), **Solana**, and **Bitcoin**; transaction construction, nonce/fee
handling, on-chain reads, and event subscription. **All signing uses cryptolib**
(Ed25519/secp via the appropriate scheme, key custody via Vault/Keyring). RPC
method names, chain IDs, and library versions are **confirmed against live sources at
implementation time**, never recalled (zero-hallucination discipline).

**Inbound — let other chains/relayers verify us, and verify them:** light-client
verification of foreign headers + Merkle-proof relay, an **IBC-style packet/relayer**
abstraction, and lock-mint / burn-release bridge patterns. This builds directly on
the §10 verification API. Bridges are a **later** milestone and get the heaviest
security review — they are historically the highest-loss component in the ecosystem.

---

## 12. Observability & event streaming

`blockchainer_observability` — first-class, not bolted on:

- **Structured logging** with levels and context (height, round, peer, tx hash).
- **Metrics:** counters/gauges/histograms (block time, mempool depth, peer count,
  validation latency, sync rate, fork/halt events) behind a pluggable sink
  (Prometheus-style exporter as one implementation).
- **Tracing:** spans across the validate → apply → commit and propose → vote →
  commit pipelines.
- **Typed event bus:** `NewBlock`, `Finalized`, `Reorg` (non-BFT engines),
  `ValidatorSetChanged`, `PeerConnected/Lost`, `MempoolAdmitted/Evicted`. This bus is
  the **streaming** API consumers subscribe to for live chain state, and what feeds
  dashboards.

---

## 13. Storage

`blockchainer_state` storage seam: a `StateStore` interface (KV with atomic batches
and height-versioned snapshots) so the backend is swappable (embedded default;
others later). Snapshots enable fast sync and clean rollback on a rejected block.
Block store and indexes (by height, by hash, by tx) are separate from the state
trie. All writes for a block commit **atomically** or not at all (§6 step 7).

---

## 14. Project layout (C++ core + C ABI + bindings)

A CMake C++ project mirroring cryptolib's structure, keeping consensus-critical code
small and auditable:

```
blockchainer/
  CMakeLists.txt              links crypto::cryptolib; builds core + C ABI + tests
  docs/                       ARCHITECTURE.md (this), THREAT_MODEL.md, CODEC_SPEC.md
  include/blockchainer/       public C++ headers
  src/
    codec/        canonical serialization (the C++ home of the removed Dart codec)
    crypto/       cryptolib adapter — Hasher/Signer/Aggregator seam (§3.2)
    core/         block/tx model, Merkle, validation pipeline, ChainSpec
    state/        StateMachine (account & UTXO) + StateStore
    consensus/    ConsensusEngine + BFT reference
    p2p/          transport, gossip, mempool, sync
    observability/ logs, metrics, tracing, event bus
    client/       full verification + light client/SPV
    interop/      EVM/Solana/Bitcoin clients + bridge primitives
    node/         assembles a runnable node from a ChainSpec
  bridge/
    blockchainer_c.h          the C ABI every binding marshals over
    blockchainer_c.cpp        exception isolation, bounds-checked length math
    bindings/
      dart/                   dart:ffi binding (+ pub package)
      go/                     cgo binding (+ module)
  tests/                      C++ suite + shared golden-vector fixtures
  vectors/                    language-neutral golden vectors (replayed by C++ & bindings)
```

Internal C++ dependencies flow **downward only**: `codec ← crypto ← core ←
{state, consensus} ← p2p ← node`; `client`, `observability`, and `interop` depend on
`core`/`crypto` but nothing depends on them. The C ABI (`bridge/`) sits above the
core; bindings sit above the C ABI and contain no logic. Consensus, codec, and the C
ABI are the highest-scrutiny units. Golden vectors in `vectors/` are consumed by both
the C++ tests and each binding's test suite (§2).

---

## 15. Testing strategy

| Layer | Discipline |
|-------|-----------|
| Codec | Round-trip + **non-canonical-rejection** property tests; **fuzz** every decoder (untrusted network input). |
| Hashing/Merkle | Golden vectors; proof verification incl. tampered-proof rejection; odd-node malleability tests. |
| Determinism | Same input → same bytes/hash across runs & platforms; **golden vectors replayed identically through the C++ core and every binding** (Dart, Go). |
| State machine | Property tests on balance/nonce/UTXO invariants; **no-overflow** value-math tests. |
| Consensus | Safety/liveness **property tests**; Byzantine-scenario tests (equivocation, withholding, `f` faulty validators); halt-not-fork assertions. |
| Networking | Adversarial peers: floods, eclipse attempts, malformed frames, replay. |
| Interop | Recorded-RPC fixtures + live smoke tests gated separately; never hardcode recalled method names. |
| End-to-end | Multi-validator local network producing & finalizing blocks; chaos (drop/delay/partition). |

---

## 16. Roadmap

| M | Milestone | Exit criteria |
|---|-----------|---------------|
| **M0** | Foundations | This doc reviewed; **CMake C++ project scaffold linking `crypto::cryptolib`**; C ABI skeleton (`bridge/blockchainer_c.{h,cpp}`); empty Dart + Go binding stubs that load and call `version()`; CODEC_SPEC drafted; CI builds C++ + both bindings. |
| **M1** | Core ledger (C++) | Canonical codec + `vectors/` golden vectors; block/tx model; Merkle; crypto seam wired to cryptolib C++; **account + UTXO** state machines behind the interface; single-node append + validate; full §6 pipeline; determinism + fuzz suites green; **golden vectors replayed through Dart & Go bindings**. |
| **M2** | BFT consensus | `ConsensusEngine` + BFT reference; single- then multi-validator local net finalizing blocks; BLS-aggregated certificates; Byzantine-scenario tests. |
| **M3** | Networking & sync | Noise-encrypted transport, gossip, mempool, fast/streaming sync; adversarial-peer tests. |
| **M4** | Observability & streaming | Logs/metrics/tracing + typed event bus + live stream API (callback/stream over the C ABI); Prometheus exporter. |
| **M5** | Verification & light client | Full verify API + SPV/light client + proof verification API, exposed through the C ABI to Dart & Go. |
| **M6** | Interop | EVM/Solana/Bitcoin outbound clients (signing via cryptolib); then inbound light-client/bridge primitives (heaviest review). |
| **M7** | Hardening | THREAT_MODEL.md; broad fuzzing (codec + C ABI); ASan/UBSan/TSan clean; profile-driven C++ optimization; security review of consensus, codec, and the C ABI bridge. |

---

## 17. Open decisions to confirm before / during M1

1. **Value field width:** fixed 128-bit unsigned vs arbitrary-precision `BigInt`
   with a pinned canonical encoding. (Leaning: pinned 128-bit for ledgers, BigInt
   internally for safety, with overflow → reject.)
2. **State trie:** Merkle Patricia (EVM-proof-compatible) vs a simpler sorted binary
   Merkle trie (simpler, our own proof format). Affects interop proof compatibility.
3. **Storage backend** for the default embedded `StateStore` (C++ embedded KV).
4. **Block timestamp policy:** allowed drift window and parent-monotonicity rule.
5. ~~cryptolib linkage~~ — **decided:** link the prebuilt `cryptolib_c` C ABI
   (§3). Remaining sub-question: how CI obtains the artifact — build cryptolib's
   `cryptolib_c` target as a step vs consume a pinned prebuilt/self-contained
   binary (cryptolib already ships `scripts/build_selfcontained.sh`).
6. **C++ test framework:** match cryptolib's zero-dependency runner (consistency)
   vs a framework (doctest/Catch2).
7. **Go binding distribution:** cgo against a built shared lib vs a vendored merged
   static archive (cryptolib already faces this; reuse its approach).

These don't block writing the codec and core model; we resolve each as its module
arrives.
```
