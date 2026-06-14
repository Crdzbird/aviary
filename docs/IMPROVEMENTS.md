# Blockchainer — Improvements & Deferred Work

> A running backlog of everything worth improving that we consciously deferred
> while building M0–M2. Nothing here is a bug in what exists (all 10 suites pass,
> sanitizer-clean); these are scope cuts, known limitations, efficiency notes,
> and open decisions to revisit. Grouped by area, with rough severity:
> **[H]** high / correctness-or-scale blocker before production · **[M]** medium ·
> **[L]** low / polish.
>
> Last updated: end of M2 (BFT consensus). See [`ARCHITECTURE.md`](ARCHITECTURE.md)
> for the design and [the status memory] for what's done.

---

## Consensus (M2)

- **[H] Full async BFT round state machine.** Round is fixed at `0`. No propose
  timeouts, prevote step, locking/POLC (proof-of-lock-change), nil-votes, or
  proposer rotation. The current engine is the safety-critical *certificate*
  core; the liveness machinery (Tendermint rounds) is not built.
  (`src/consensus/bft_engine.cpp`)
- **[H] Validator-set changes.** No staking, bonding/unbonding, epoch
  transitions, governance, or anti-downgrade overlap rule. The set is fixed at
  construction.
- **[H] Equivocation evidence & slashing.** We *prevent* conflicting
  finalization (quorum intersection), but a double-signing validator is not
  detected, evidenced, or punished. Need evidence types + a slashing hook.
- **[M] Extract the abstract `ConsensusEngine` interface.** `BftEngine` is
  concrete. Extract the interface from `ARCHITECTURE.md` §7 when a second engine
  (PoS/PoW) lands, so the abstraction is informed, not guessed.
- **[M] PoS and PoW reference engines.** Designed as pluggable; not implemented.
  These also need a real fork-choice rule (BFT doesn't reorg, so none exists yet).
- **[M] Embed the commit certificate in the block header (LastCommit pattern).**
  The certificate is currently passed alongside the block, not carried in the
  next header. Embedding it makes the chain self-justifying for sync/light
  clients. Revisit `consensus_data` usage and `block_id` definition when doing so.
- **[L] `FastAggregateVerify` option.** Votes bind the validator index so BLS
  *core* `AggregateVerify` (distinct-message) applies — rogue-key-safe, no PoP,
  no cryptolib change. A same-message `FastAggregateVerify` in cryptolib would be
  ~1 pairing vs N and shrink the signed message; weigh the perf win vs the new C
  ABI surface. (See the design note in the status memory.)
- **[L] `ValidatorSet` construction validation.** Does not reject duplicate
  pubkeys, zero-power validators, or wrong-length (≠48B) BLS keys. Validate at
  construction. (`src/consensus/consensus.cpp`)
- **[L] `aggregate_commit` signature re-sort is O(n²).** Re-scans `votes` per
  signer to order signatures. Zip (index, sig) instead. Negligible for small N.
- **[L] Distinct domain tag for the validator-set hash.** `ValidatorSet::hash()`
  reuses `Domain::kHeader`. Give consensus structures their own tag(s).

## State machines & ledger (M1)

- **[H] Incremental/persistent state commitment.** `state_root()` rebuilds a
  binary Merkle tree over *all* sorted leaves every call — O(state size) per
  block. Production needs an incremental Merkle/Patricia/Verkle trie with
  copy-on-write. (`account_state.cpp`, `utxo_state.cpp`)
- **[H] `clone()` is a full deep copy.** Block processing clones the entire state
  for atomicity (`Ledger::process_block`). Fine for tests, O(state) per block at
  scale — want a persistent/COW structure or a journaled overlay.
- **[H] Fees / gas.** Neither model charges fees. UTXO enforces *exact* value
  conservation (no fee output); account has no fee/nonce-priced execution. No fee
  market or mempool priority follows from this.
- **[M] State inclusion proofs.** `StateMachine::prove()` from the design is not
  implemented for either model. Needed for light clients (M5) and bridges (M6).
- **[M] Multi-signer UTXO.** A UTXO tx has a single `author`, so all inputs must
  share one owner. Real UTXO needs per-input signatures / scripts.
- **[M] Receipts / execution results + receipt root.** Header has the field
  designed but no receipts are produced.
- **[M] Formal genesis.** Genesis is ad-hoc (`credit_genesis` /
  `add_genesis_utxo`). No genesis file/spec format, no genesis state root or
  validator set bound into `ChainSpec`. `ChainSpec` is minimal (chain_id +
  state_model) — add consensus params, genesis hash, validator set.
- **[L] Account address width.** `address_of` = full 32-byte BLAKE3(pubkey).
  Revisit a 20-byte (EVM-style) form for interop (M6).
- **[L] Block timestamp policy.** Only `timestamp >= parent` is enforced. No
  upper drift bound or median-time rule (`ARCHITECTURE.md` §17.4).
- **[L] Block size / tx-count limits.** Beyond codec ceilings, the pipeline
  enforces no max block size or max txs/block (DoS surface for a real network).

## Codec (M1)

- **[L] `write_u64` caps at 2^63-1.** Full unsigned 64-bit values must go through
  `u128`/`biguint`. Add a true u64 path if a field needs it.
- **[L] `biguint` minimality is caller's responsibility on write.** Add a
  `from_integer` helper that produces the canonical minimal magnitude.
- **[L] Fixed-width, not varint.** Deterministic and simple but larger on the
  wire. Acceptable; note if size matters.

## Crypto seam (M1/M2)

- **[H] PQ / hybrid signatures in the tx path.** Only Ed25519 is wired for
  transactions. The hybrid Ed25519+ML-DSA option (cryptolib has it) and
  `ChainSpec`-driven suite selection are designed but not implemented — no PQC
  posture yet despite harvest-now-decrypt-later being in the threat model.
- **[M] Crypto seam as interfaces.** `Hasher`/`Signer`/`Verifier`/`Aggregator`
  are free functions, not the injectable interfaces of `ARCHITECTURE.md` §3.2.
  Make them interfaces if we need suite-by-name selection or fake hashers in tests.

## C ABI & language bindings

- **[x] Expose the Ledger + BftEngine through the C ABI — DONE.** `DemoNode` (full chain: ledger+BFT+mempool+observability) is exposed via an opaque handle (`blockchainer_node_*`: create/credit/new_account/submit_transfer/produce_block/height/balance/nonce/state_root/drain_events/metrics/prove_account) + interop address derivation. A high-level Dart binding (`blockchainer.dart`) and a **Flutter example app** (`examples/blockchainer_demo`) drive all of it. Remaining: multi-validator nodes, EVM/BTC tx-signing over the C ABI.
- **[M] Packaged, auto-loading binding distribution.** Bindings load
  `libblockchainer_c.dylib` by path (dev only). Mirror cryptolib's packaging: a
  pub package (bundled per-platform binary) and a Go module with a vendored/
  static lib. Currently macOS-arm64 only; rpath is an absolute local path.
- **[M] C ABI error taxonomy.** Errors are generic ints / strings. Add stable
  error codes (no oracle) per the boundary contract.
- **[L] Bindings hardcode golden vectors.** Have the Dart/Go demos parse
  `vectors/*.txt` instead of embedding the hex, so vectors have one source.

## Networking (M3 — core done, these deferred)

The M3 work built the *transport-agnostic protocol logic* (mempool, wire
protocol, secure session) and proved it with a two-node in-memory integration
test. Still to do:

- **[H] Real socket transport + async event loop.** Everything runs over an
  in-memory transport today. Need an actual TCP (or QUIC) transport, connection
  lifecycle, and an async I/O loop, behind a `Transport` interface so the logic
  stays testable.
- **[H] Multi-validator vote gossip.** The integration test finalizes with a
  single validator (quorum 1). Collecting precommits across the network needs the
  async BFT round state machine (also deferred, under Consensus) plus a
  vote-gossip message + aggregation across peers.
- **[M] Gossip overlay manager.** Wire messages exist, but no mesh: seen-set
  dedup cache, flood/rate control, message TTL/hop limits, eclipse-resistant peer
  selection, announce-then-fetch (Inv/GetData).
- **[M] Peer discovery & address book.** No discovery, handshake/`Hello`
  negotiation beyond the struct, peer scoring, or ban list.
- **[M] Mutual identity authentication on the secure session.** The channel is
  confidential + PQ-hybrid + forward-secret but does not yet bind to a known
  peer/validator identity (MITM on first contact). Layer a signed handshake
  transcript, or bridge cryptolib's Noise XX (C++-only) to the C ABI.
- **[M] Streaming state-snapshot fast sync.** Sync is block-by-block. Large-state
  fast sync (stream a state snapshot over SecretStream, then tail blocks) is not
  built — the SecretStream wrappers are ready for it.
- **[L] Mempool maturity.** Fee-priority eviction (needs fees), rebroadcast,
  expiry/TTL, per-peer rate limiting & DoS accounting. The O(n²) selection
  fixed-point is fine for now (see also the no-fees note above).
- **[L] Keepalive / backpressure.** No Ping/Pong, no flow control on the stream.

## Observability (M4 — core done, these deferred)

M4 built a typed event bus (the streaming API), a metrics registry with a
Prometheus text exporter, and structured logging, wired into the BFT engine and
mempool. Deferred:

- **[M] Network-exposed streaming + `/metrics` endpoint.** The event bus is
  synchronous in-process; metrics produce text but there's no HTTP `/metrics`
  scrape endpoint or external event-stream transport (websocket/gRPC) for
  off-process consumers (dashboards). No async delivery or backpressure.
- **[M] Real distributed tracing.** Only events + metrics exist; no spans with
  parent/child context, trace ids, or propagation across the pipeline as
  ARCHITECTURE §12 envisions.
- **[L] Metric labels/dimensions.** Metrics are name-only (no `{label="..."}`
  series). Registry creation isn't thread-safe (the metrics themselves are);
  no metric removal.
- **[L] Wider instrumentation.** Only the engine (BlockFinalized/Rejected) and
  mempool (TxAdmitted/Removed) emit. Ledger/consensus/p2p emit no logs/traces;
  events like PeerConnected/ValidatorSetChanged/Reorg are defined-or-implied but
  not emitted. `BlockRejected` carries no reason field.
- **[L] Logging maturity.** No JSON formatter, async sink, rotation, or a global
  default logger; sinks aren't yet attached inside core components.

## Verification & light client (M5 — core done, these deferred)

M5 built the light client (header-chain verification via commit certificates),
state + transaction inclusion proofs, and exposed proof/commit verification
through the C ABI (verified cross-language in Dart & Go). Deferred:

- **[M] Absence (non-membership) proofs.** Only presence proofs exist;
  `verify_state_proof` returns false for `present=false`. Proving a key is *not*
  in the state needs neighbor proofs over a sorted trie (ties into the
  incremental-trie item under State machines).
- **[M] Stateful light client over the C ABI.** The bridge exposes blob-based
  `verify_state_proof` / `verify_tx_inclusion` / `verify_commit`, but not a
  `LightClient` handle that follows a header chain across calls. Dart/Go can
  verify individual artifacts, not run a following light client.
- **[M] Header-only sync protocol.** `LightClient::accept` is one header at a
  time; the wire protocol has `GetBlocks` (full blocks) but no headers-only sync
  message for light clients to batch-download and verify headers.
- **[L] UTXO state proof not exercised cross-language.** Only the account proof
  has a golden vector replayed through the bindings.
- **[L] Proof formats unversioned.** Add a version byte to StateProof /
  TxInclusionProof for forward compatibility.

## Interoperability (M6 — EVM tx core done, much deferred)

M6 added the interop crypto primitives (keccak256, ripemd160, secp256k1 — added
to cryptolib, verified by KATs), RLP, and the EVM transaction + ABI core
(addresses + EIP-55, legacy EIP-155 sign/recover, static ABI encode/decode +
selectors — validated against real-world vectors). Deferred:

- **[H] Real HTTP transport.** The JSON-RPC client + JSON module + typed
  `EthClient` (blockNumber/chainId/gasPrice/getBalance/getTransactionCount/
  call/sendRawTransaction) are **done** and tested over a mock transport. What's
  missing is a production `RpcTransport` doing actual HTTP(S) POST (+ retries,
  timeouts, batching). Method names / chain-ids should still be **confirmed
  against live docs** at integration. More `eth_*` methods (getTransactionReceipt,
  getLogs, estimateGas, getBlockByNumber) as needed.
- **[M] Trustless (light-client) bridge.** The **federated** bridge is DONE: IBC-style packets, BLS-attested (>2/3 attestor set), lock-mint / burn-release, replay protection, escrow accounting — all tested (`test_bridge`). The upgrade is a *trustless* path: verify foreign consensus + a Merkle-Patricia inclusion proof instead of trusting an attestor federation; slots in behind the same "verify before mint" seam (RLP+keccak in place). Bridges remain the highest-risk component → heaviest review before production.
- **[L] Solana interop — DONE for transfers.** Base58/base64/compact-u16, Solana
  addresses, System-Program transfer message build + Ed25519 sign, and a
  SolanaClient (getBalance/getLatestBlockhash/sendTransaction) over the mock
  transport are built and tested. Remaining: versioned (v0) messages + address
  lookup tables, arbitrary instructions/programs, durable nonces, more RPC
  methods, and a real HTTP transport (shared with EVM).
- **[L] Bitcoin interop — addresses + P2WPKH signing DONE.** HASH160/256,
  Base58Check, Bech32, P2PKH/P2WPKH addresses, and **BIP143 SegWit signing**
  (sighash + DER + witness assembly + SegWit serialization) are byte-exact vs the
  BIP143 spec example. Remaining: legacy/P2SH sighash, Taproot (BIP341)/bech32m,
  PSBT, coin-selection/fee helpers, and a Bitcoin RPC/Esplora client.
- **[M] EIP-1559 / typed (EIP-2718) transactions.** Only legacy (type-0) txs.
- **[M] Dynamic ABI types.** Only static (address/uintN/bool/bytes32); no
  string/bytes/arrays/tuples (head-tail offset encoding).
- **[L] Full uint256.** ABI/value use u128; large token amounts (>2^128) need a
  256-bit path.
- **[L] C ABI exposure of interop.** Dart/Go can't yet build/sign an EVM tx
  through the bridge.

## Storage (design §13, not started)

- **[H] No persistence.** All state is in-memory `std::map`. No `StateStore`
  abstraction, no block store, no by-height/by-hash/by-tx indexes, no
  height-versioned snapshots or on-disk rollback. Pick the embedded backend
  (`ARCHITECTURE.md` §17.3).

## Testing, fuzzing & CI

- **[x] CI — DONE (workflow + script).** `git init` done; `scripts/ci.sh` runs C++ build + ctest + ASan/UBSan + Dart/Go bindings + Flutter analyze (verified green locally); `.github/workflows/ci.yml` runs it on macOS (builds cryptolib_c, set `CRYPTOLIB_REPO`). Remaining: a timed libFuzzer run in CI, Linux matrix, caching.
- **[M] Continuous fuzzing + corpus.** `fuzz/fuzz_codec.cpp` exists but is
  opt-in and uncorporated. Run it in CI with a seed corpus; add fuzz targets for
  the consensus and block decoders.
- **[M] TSan.** No ThreadSanitizer pass (no threading yet — add when networking
  lands).
- **[L] Cross-language coverage is hash-level only.** Dart/Go agree on
  hash/txid/header_hash, not full block processing (single C++ impl makes deep
  differential testing unnecessary, but note the boundary of what's proven).
- **[L] No benchmarks.** `ARCHITECTURE.md` §8 native-path work is profile-driven;
  no profiling/benchmarks exist yet.

## Build & packaging

- **[M] cryptolib consumption.** Found via sibling path + prebuilt dylib. No
  `FetchContent`/version pin, no `find_package` config, no relocatable rpath
  (absolute local path baked in). (`CMakeLists.txt`; `ARCHITECTURE.md` §17.5)
- **[M] No install/export for blockchainer itself.**
- **[L] Go binding distribution** unresolved (`ARCHITECTURE.md` §17.7): cgo vs
  vendored static archive.

## Not-yet-started milestones (tracked in the roadmap)

- **M3** networking & sync — *core done* (mempool, wire protocol, secure session,
  two-node integration); see the Networking section above for what's deferred
- **M4** observability & streaming — *core done* (event bus, metrics+Prometheus,
  logging, wired into engine/mempool); see the Observability section for deferred
- **M5** verification & light client — *core done* (light client, state/tx
  inclusion proofs, C-ABI verification cross-language); see the Verification
  section for deferred
- **M6** interop — *EVM read+write; Solana transfers; Bitcoin addresses + P2WPKH signing* (interop crypto, RLP/ABI/EthClient, base58/base64/compact-u16, Solana transfer+client, Base58Check/Bech32 + BTC addresses + BIP143 signing — over mock transport); remaining: real HTTP(S) transport, trustless light-client bridge upgrade
- **M7** hardening — *in progress*: THREAT_MODEL.md, interop fuzzing, full-suite ASan/UBSan clean, 2 findings fixed, **CI (git+script+workflow) + full C-ABI chain exposure + Flutter example app**; remaining: TSan, libFuzzer corpus, external audit

## Resolved decisions (recorded so we don't relitigate)

- Value field width → **u128 fixed 16 bytes** (`ARCHITECTURE.md` §17.1). Revisit
  only if a chain needs arbitrary-precision values.
- C++ test framework → **custom zero-dependency runner** (`tests/check.hpp`),
  matching cryptolib (§17.6).
- cryptolib linkage → **prebuilt `cryptolib_c` C ABI**, not the header-only C++
  API (§3). CI-acquisition sub-question still open (above).
- BLS same-message problem → **bind validator index in the vote** (above), no
  cryptolib change.

## Sentinel demo follow-ups (examples/sentinel)
- Anchoring overloads `recipient = contentHash` (a 1-unit transfer). Cleaner long-term: a dedicated `anchor(hash, metadataHash)` tx body in the account state machine so anchors don't masquerade as transfers / consume balance. Spec for cryptolib/core if pursued.
- StreamData seals on a fixed 1.5s ticker; make the cadence/threshold configurable and add backpressure if a device streams faster than blocks seal.
- Credential is currently bound into the fingerprint only; no proof-of-possession challenge. Add a signed nonce challenge at ProvisionDevice for real device attestation.
- Media bytes are sent whole over gRPC to be hashed server-side; for large video, hash client-side in chunks (Merkle of chunks) and stream only roots.
