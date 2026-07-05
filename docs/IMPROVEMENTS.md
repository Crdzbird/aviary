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

- **[~] Full async BFT round state machine — CORE DONE.** `RoundStateMachine`
  (`round_state.hpp`) is a faithful, deterministic transcription of Tendermint
  Algorithm 1: propose/prevote/precommit steps, deterministic proposer rotation,
  round-change on timeouts, nil votes, and the locking / valid-value (POLC) rules
  that preserve safety across rounds. It is a pure state machine (no clock/net/
  crypto) — the driver feeds authenticated messages + timeout events and performs
  the returned `Action`s (broadcast / schedule-timeout / decide). `test_round_
  state.cpp` runs an N-validator in-memory simulation: agreement + termination on
  the happy path, round-change + termination with a faulty round-0 proposer, a
  silent minority (f=2 of 7), a 10-validator set, and locking; ASan/UBSan-clean.
  Added ALONGSIDE the existing single-shot `BftEngine` (which `DemoNode` still
  uses). **Driver DONE:** `RoundDriver` (`round_driver.hpp`) binds the SM to the
  real world — it builds/validates/commits blocks through the `Ledger`, signs and
  verifies BLS votes against the validator set, aggregates a real >2/3
  `CommitCertificate` on decide, and gossips `wire::ConsensusMsg`. It's still a
  pure input/output object (returns messages to gossip + timeouts to arm), so a
  multi-node in-memory network is deterministic and testable. `test_round_
  driver.cpp`: 4 validators commit the SAME real block (identical state root) with
  a certificate that `verify_commit`s; a faulty round-0 proposer still commits
  after a round change; 7 validators with a silent f=2 minority commit. ASan-clean.
  **Multi-height DONE:** `RoundDriver::advance()` rolls a committed height into the
  next — it carries that height's certificate as the new block's LastCommit,
  resets the round SM, and StartRound(0)s. The chain self-certifies:
  `test_round_driver.cpp` runs heights 1→4 and verifies each block's embedded
  LastCommit is a valid >2/3 certificate for its parent (right height + block id),
  with all ledgers agreeing on the tip hash + state root, incl. a 3-of-4 run
  chaining three heights through a silent validator. **Remaining:** a real
  socket/timer transport, a proposer-rotation policy beyond round-robin, and
  equivocation handling within the round SM.
- **[H] Validator-set changes.** No staking, bonding/unbonding, epoch
  transitions, governance, or anti-downgrade overlap rule. The set is fixed at
  construction.
- **[~] Equivocation evidence & slashing — DETECTION DONE.** `Evidence` +
  `detect_equivocation` (scan precommits at one height/round for a validator that
  signed two different block ids) + `verify_equivocation` (both signatures valid
  under the signer's key, distinct block ids) prove a double-sign; `ValidatorSet::
  with_slashed(i)` zeroes the offender's power (`test_consensus_hardening.cpp`).
  **Remaining:** gossip evidence over the wire and *apply* slashing as a
  validator-set change at a height (needs the validator-set-change machinery
  below) + an economic/stake layer for the actual penalty.
- **[x] Extract the abstract `ConsensusEngine` interface — DONE.**
  `consensus_engine.hpp` defines the family-agnostic seam — `ledger()` /
  `height()` / `tip()` / `propose()` plus `submit(block, justification)` where the
  justification is an OPAQUE byte string (a BFT commit certificate, a PoW
  solution, a PoS finality proof — each engine defines + verifies its own). The
  now-concrete `BftEngine` implements it (`submit` decodes a `CommitCertificate`
  and finalizes), keeping its BFT-specific `precommit`/`try_commit`/`finalize` as
  extras. The pipeline above consensus can be written once against the interface;
  a second engine (PoW/PoS) slots in underneath. `test_bft.cpp` drives a chain
  through a `ConsensusEngine&`.
- **[~] PoS reference engine — DONE.** `PosEngine` (`pos_engine.hpp`) implements
  `ConsensusEngine` as a THIRD family: each height has one deterministic,
  stake-weighted **slot leader**; the block's justification is that leader's BLS
  signature over the block id (a third justification type — distinct from the BFT
  certificate and the PoW nonce, all behind the same `submit`). More stake → more
  slots; a slashed (0-power) validator never leads (reusing the stake + slashing
  machinery). **Decoupling:** proposer selection is an injectable `ProposerPolicy`
  (default stake-weighted; swappable for round-robin / VRF / epoch schedule
  without touching the engine). `submit` verifies the block was produced by the
  legitimate leader for its height AND the leader's signature, then commits.
  `test_pos.cpp`: stake-weighted distribution (~70/20/10, 0-stake excluded),
  a 3-validator chain reaching agreement, adversarial rejection (wrong leader /
  tampered sig / impostor index), a round-robin policy swap, and driving via a
  `ConsensusEngine&`; PosData fuzzed. Reference-grade (no fork choice / no
  finality gadget beyond the leader signature).
- **[~] PoW reference engine — DONE.** `PowEngine`
  (`pow_engine.hpp`) implements `ConsensusEngine` as a second consensus family: a
  block is justified by *work* — its header hash must be <= a difficulty target,
  with the nonce carried in `consensus_data` (so it is committed by the hash).
  `propose` yields an unsolved candidate, `mine` searches nonces, `submit`
  re-verifies the embedded work (target + hash<=target) then commits.
  `test_pow.cpp`: mine+submit, insufficient-work + wrong-target rejection, driving
  a chain through a `ConsensusEngine&`, PowData fuzzed. It is a *reference* engine:
  fixed difficulty (no retargeting) and linear extension. **Remaining:** a
  difficulty-retargeting rule.
- **[x] Fork choice + reorg — DONE.** `ForkChoice` (`fork_choice.hpp`) is an
  engine-agnostic block tree: `add_block(block, weight)` validates the block's
  state transition on its parent (via the Ledger), stores it with cumulative
  weight, and keeps the canonical head on the heaviest branch — **reorganizing**
  (rebuilding the ledger by replaying the winning branch from genesis) when a
  side branch overtakes. The per-block weight is caller-supplied, so the rule is
  decoupled from consensus (PoW passes work, PoS/BFT pass 1 = longest-chain);
  ties keep the incumbent (deterministic). `test_fork_choice.cpp`: linear
  extension, a stored-but-not-canonical side branch, an **observable state
  reorg** (a heavier branch flips the balances), a fast extension of the new
  head, and rejection of duplicate / unknown-parent / invalid-transition blocks.
  ASan-clean. **Remaining:** an incremental reorg (snapshot the common ancestor
  instead of replaying from genesis — the state-snapshot primitives make this
  feasible), and finality/checkpoint pruning of the tree.
- **[x] Embed the commit certificate in the block header (LastCommit) — DONE.**
  `BftEngine::propose` stamps `consensus_data` = `ConsensusData{validator_set_hash,
  last_commit}` (the certificate that finalized the parent), committed by the
  header hash so validators sign it. `check_finalization` and `LightClient::accept`
  now run `validate_consensus_data`: the validator-set hash must match (anti
  vset-swap) and the embedded last commit must be a valid >2/3 certificate for
  the parent (height-1). The first block after genesis carries none. The chain is
  now self-certifying — a follower confirms every ancestor was *finalized*, not
  merely linked (`test_consensus_hardening.cpp`, `test_verification.cpp`).
- **[L] `FastAggregateVerify` option.** Votes bind the validator index so BLS
  *core* `AggregateVerify` (distinct-message) applies — rogue-key-safe, no PoP,
  no cryptolib change. A same-message `FastAggregateVerify` in cryptolib would be
  ~1 pairing vs N and shrink the signed message; weigh the perf win vs the new C
  ABI surface. (See the design note in the status memory.)
- **[x] `ValidatorSet` construction validation — DONE.** The constructor now
  rejects an empty set, a wrong-length (≠48B) BLS key, duplicate public keys, and
  a set with zero total voting power (a single zero-power/slashed validator among
  live ones is still allowed). `test_consensus_hardening.cpp`.
- **[x] `aggregate_commit` signature re-sort O(n²) → O(n log n) — DONE.** Now zips
  (index, signature) and sorts once; no per-signer re-scan.
- **[x] Distinct domain tag for the validator-set hash — DONE.** `ValidatorSet::hash()`
  now uses `Domain::kValidatorSet` (0x04), separated from `kHeader`.

## State machines & ledger (M1)

- **[x] Incremental state commitment — DONE (account model).** `AccountStateMachine`
  now commits copy-on-write: the committed accounts + their cached Merkle tree
  live in an immutable, shared `Snapshot`; uncommitted changes sit in a small
  `overlay_`. `state_root()` for a block that moves value between existing
  accounts recomputes only the changed leaves' root-paths against the cached
  tree via `merkle::MerkleIndex::root_with_updates` — O(K·log N), byte-identical
  to `compute_root`. New accounts / a large overlay trigger a rebuild, amortised
  by sealing above ~1/8 of the state. `clone()` shares the snapshot and copies
  only the overlay → O(changed), not O(state). Differential + property tests
  (`test_state_incremental.cpp`, 4000-op naive cross-check, MerkleIndex vs
  `compute_root`, COW independence). Benchmark (`tools/bench_state.cpp`): the
  per-block commit is flat as accounts grow (208µs→488µs over 1k→256k accounts)
  vs the old full rebuild (4.6ms→799ms) — up to ~1600× at 256k. **UTXO model DONE
  too:** `UtxoStateMachine` uses the same shared-`Snapshot` + `overlay_` (a
  tombstone marks a spent outpoint), so `clone()` is O(changed) not O(state).
  UTXO blocks are inherently structural (add/remove outpoints), so `state_root()`
  reseals rather than incrementally updating — but the committed root is
  byte-identical and clone no longer scales with the set size
  (`test_utxo.cpp` 600-op COW differential + clone independence). **Remaining:**
  optional persistent map so `clone()` sheds its residual O(N) map copy at seal.
- **[~] Fees / gas.** **Account model DONE (flat base + variable tip):**
  `ChainSpec.flat_fee` (default 0 → feeless, byte-identical to no-fee) is a fixed
  base fee, and the v2 envelope carries an explicit per-tx `fee` (priority tip,
  committed by signing_bytes). `AccountStateMachine::validate/apply` charge
  `flat_fee + tip` (overflow-guarded, burned). The mempool is now a real fee
  market: **fee-priority eviction** (a full pool drops its cheapest resident only
  if the newcomer outbids it) and **fee-ordered selection** (highest tip offered
  to the block first) — `test_ledger.cpp` + `test_mempool.cpp`. v1 txs pay tip 0,
  so nothing changes for them. The tip is exposed across every binding
  (`SubmitTransferFee`/`submitTransferFee`) via `blockchainer_node_submit_transfer_fee`.
  **Remaining:** gas metering for richer execution; a variable per-tx fee for the
  UTXO model + how the envelope tip interacts with its implicit inputs−outputs fee.
- **[M] State inclusion proofs.** `StateMachine::prove()` from the design is not
  implemented for either model. Needed for light clients (M5) and bridges (M6).
- **[~] Multi-signer UTXO — DONE (per-input witnesses).** A new `SigAlgo::
  kUtxoWitness` (version-2 envelope, empty author) carries one `{pubkey,
  signature}` witness per input in `Transaction::signature`; each signs the tx's
  signing_bytes and `UtxoStateMachine::validate` verifies it against that input's
  owner, so a single tx can spend inputs owned by DIFFERENT parties.
  `verify_transaction_signature` defers witness txs to the state machine (the
  account model rejects them — empty author — so they only apply on a UTXO
  chain). Single-author v1 UTXO and the account model are unchanged.
  `test_utxo_multisig.cpp`: two-owner spend, wrong-signer / tampered-witness /
  wrong-count rejection, fee-paying multisig; witness decoder fuzzed; ASan-clean.
  **Remaining:** full script/covenant support (this is single-sig-per-input,
  SIGHASH_ALL) and segwit-style witness-excluded txids to kill malleability.
- **[x] Receipts / execution results + receipt root — DONE.** A per-tx `Receipt`
  (status + fee) is committed via a `receipt_root` in a **version-2** BlockHeader
  — version-gated so v1 encodings (and golden vectors) are byte-identical, and
  opt-in via `ChainSpec.produce_receipts` (default off). `StateMachine::tx_fee`
  reports the pre-apply fee (account: flat + declared tip; UTXO: inputs−outputs);
  `Ledger` builds receipts in `build_block`/`process_block`, commits the root,
  validates it on receive (`receipt_root mismatch` → reject), and exposes
  `last_receipts()`. Receipt-inclusion proofs (`receipt_inclusion_proof` /
  `verify_receipt_inclusion`) let a light client prove a tx executed with a given
  result against a verified header. `test_receipts.cpp` (both models, proof +
  tamper, v1 unchanged), fuzzed, ASan-clean; Go golden vectors still replay.
- **[~] Formal genesis — DONE (account model).** `GenesisSpec` (`genesis.hpp`) is
  the canonical, hashable description of a chain's initial conditions: chain
  params (chain_id, state_model, accepted_sig_algos, flat_fee, block limits,
  timestamp), the genesis accounts, and the validator set. `encode`/`decode_all`
  are canonical (accounts are address-sorted, so the hash is insertion-order
  independent); `hash()` (domain `kGenesis`) is the chain's identity; `chain_spec()`
  derives the runtime `ChainSpec`; `validator_set()` recovers the vset;
  `ledger_from_genesis()` builds a seeded `Ledger`. Same spec → same hash + state
  root + params (`test_genesis.cpp`, fuzzed). **Remaining:** UTXO genesis outputs,
  and binding the genesis hash + validator set into a chain handshake / the
  `ChainSpec` used by the running node.
- **[L] Account address width.** `address_of` = full 32-byte BLAKE3(pubkey).
  Revisit a 20-byte (EVM-style) form for interop (M6).
- **[L] Block timestamp policy.** Only `timestamp >= parent` is enforced. No
  upper drift bound or median-time rule (`ARCHITECTURE.md` §17.4).
- **[x] Block size / tx-count limits — DONE.** Consensus-level ceilings
  (`ChainSpec.max_block_txs` / `max_block_bytes`, 0 = unlimited) are enforced on
  EVERY block in both `Ledger::build_block` (producer) and `process_block`
  (receiver, before per-tx work) — a peer cannot force O(huge) verification with
  one oversized block (`test_ledger.cpp`). Mempool-side, `select()` stops at
  whichever binds first — the tx-count `max` or `MempoolConfig.max_block_bytes` —
  and a per-account pending cap (`max_per_account`, default 64) stops one sender
  flooding the pool and starving others (`AddResult::kAccountLimit`,
  `test_mempool.cpp`).

## Codec (M1)

- **[L] `write_u64` caps at 2^63-1.** Full unsigned 64-bit values must go through
  `u128`/`biguint`. Add a true u64 path if a field needs it.
- **[L] `biguint` minimality is caller's responsibility on write.** Add a
  `from_integer` helper that produces the canonical minimal magnitude.
- **[L] Fixed-width, not varint.** Deterministic and simple but larger on the
  wire. Acceptable; note if size matters.

## Crypto seam (M1/M2)

- **[x] Custodial key management (S3) — DONE.** `blockchainer::custody`
  (`keyring.hpp`) wraps cryptolib into three pieces: `Vault` (a committing-AEAD
  envelope-encryptor keyed by a 32-byte master, AAD-bound, stateless so a blob
  seals now and opens after reload), `Keyring` (a random master key wrapped by
  device / passphrase slots — the same master unlocks through any slot;
  serialise/deserialise), and `KeyStore` (account signing secrets sealed at rest,
  keyed by address with the address bound as AAD; serialise/load returns it
  *locked*; unlock with a passphrase/device factor to reveal a key for signing).
  A node need never persist a plaintext key. `test_keyring.cpp` proves the slot
  equivalence, seal/open tamper + wrong-key + wrong-AAD rejection, the "no
  plaintext in the serialised blob" property, reload→unlock→reveal, and a
  revealed key producing a valid signature; ASan/UBSan-clean. The KeyStore is
  now exposed through the C ABI (`blockchainer_keystore_*`) and wrapped in every
  binding (Go/Dart/npm/Swift/JVM) as a `KeyStore` class — custodial signing is
  `reveal(address) → submitTransfer(secret, …)`, verified end-to-end on all four
  runtime bindings (`alice 750 / bob 250`, wrong-passphrase rejected) + a durable
  Go test (`TestKeyStoreCustodialSigning`). **Remaining:** zeroize master/secret
  buffers on drop; an optional `DemoNode` convenience that signs by address.
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
- **[~] Gossip overlay manager — CORE DONE.** `p2p::GossipOverlay` (`gossip.hpp`) is the mesh on top of the wire/secure-session transport: it dedups messages by content hash (BLAKE3) and floods a new one to peers except the sender, so a message reaches every connected node exactly once over any (partial) topology, loop-free. Transport-agnostic (returns relays to send). `test_gossip.cpp`: ring, partial mesh, and clique all propagate to every node with the redundant paths deduped. **Remaining:** peer scoring / ban list, message TTL + expiry of the seen-set, and topic-scoped subscriptions.
- **[M] Peer discovery & address book.** No discovery, handshake/`Hello`
  negotiation beyond the struct, peer scoring, or ban list.
- **[x] Mutual identity authentication on the secure session — DONE.** The
  channel now binds to long-term identities via a SIGMA-style signed handshake
  transcript: each side signs (KEM ciphertext ‖ both stream headers ‖ both
  identity keys ‖ a channel binder derived from the shared secret) with a hybrid
  Ed25519+ML-DSA-65 key. Verification failure aborts the handshake, and
  `peer_identity()` exposes the *authenticated* remote key for allow-list
  pinning. Authentication is **post-quantum** (the ML-DSA half can't be forged
  even by a quantum adversary who breaks X25519/Ed25519), so "harvest now,
  impersonate later" MITM is prevented. `test_secure_session.cpp` covers
  mutual-auth, forged-initiator/responder rejection, wrong-KEM-key abort, and
  impostor-key visibility. **Remaining:** identity negotiation in the `Hello`/
  peer-discovery layer (who to expect) and bridging the session to the C ABI.
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
- **[~] EIP-1559 / typed (EIP-2718) transactions — DONE (type-2).** `evm::Eip1559Tx` + `sign`/`recover_sender`/`signing_hash`: the EIP-2718 type-2 dynamic-fee tx (chainId, nonce, maxPriorityFeePerGas, maxFeePerGas, gas, to, value, data, accessList), RLP payload prefixed with 0x02, keccak signing hash, secp256k1 signature with a 0/1 y-parity (not the legacy v), and an access list (address + storage keys). `test_evm.cpp`: envelope shape, sign→recover round-trip (incl. contract creation + non-empty access list), determinism, and field-commitment. **Remaining:** type-1 (EIP-2930) and type-3 (EIP-4844 blob) txs, and full uint256 for values > 2^128.
- **[M] Dynamic ABI types.** Only static (address/uintN/bool/bytes32); no
  string/bytes/arrays/tuples (head-tail offset encoding).
- **[L] Full uint256.** ABI/value use u128; large token amounts (>2^128) need a
  256-bit path.
- **[L] C ABI exposure of interop.** Dart/Go can't yet build/sign an EVM tx
  through the bridge.

## Storage (design §13, partially started)

- **[~] Persistence — SNAPSHOT SERIALIZATION DONE.** The committed state now
  serializes to a self-describing byte blob: `StateMachine::serialize()` +
  `deserialize_state(model, bytes)` for both models (account = flat_fee +
  address-sorted {addr,balance,nonce}; UTXO = flat_fee + genesis_counter +
  outpoint-sorted {outpoint,output}). `Ledger::snapshot()` bundles the tip header
  + state; `Ledger::restore(spec, blob)` rebuilds a ledger at the snapshotted
  height that resumes the chain (accepts the next block identically). Round-trips
  to a byte-identical state root, fuzzed, ASan-clean (`test_snapshot.cpp`). A node
  can now write the blob to disk and reload — the foundation for a real store and
  for streaming state-snapshot fast sync.
- **[~] Block store — DONE (in-memory + serializable).** `BlockStore`
  (`block_store.hpp`) archives finalized blocks + their commit certificates
  contiguous by height, indexed by block hash and by transaction id
  (`by_height`/`by_hash`/`find_tx`); `put` enforces contiguity + no overwrite.
  `serialize()`/`load()` round-trip the whole archive (indexes rebuilt on load),
  so a node persists its chain and reloads it on restart. Fuzzed, ASan-clean
  (`test_block_store.cpp`). Together with `Ledger::snapshot` this gives full
  chain persistence to bytes.
- **[~] On-disk backend — DONE (crash-safe file store).** `storage::FileStore`
  (`storage.hpp`) is a key→bytes directory backend with **atomic writes** (temp
  file + rename, so a crash mid-write never corrupts an existing value);
  `storage::save_chain`/`load_chain` persist the block archive + ledger state
  snapshot and reload them, so a node survives restart. `test_storage.cpp`:
  put/get/has/remove, and a chain finalized through the BFT engine → saved to a
  temp dir → in-memory objects dropped → reloaded from disk → resumed (block 4
  applies), with balances/nonces/tip/certs all intact; ASan-clean. **Remaining:**
  an embedded KV engine (LMDB/RocksDB) for large state instead of whole-file
  rewrites, height-versioned snapshots + on-disk rollback, incremental
  snapshotting, and a write-ahead log for the mempool.

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

## Security hardening — Phase 1 (S1/S2) — DONE (2026)
- **[x] Genesis-seal-on-read (fund-loss bug) FIXED.** Reads (height/balance/state_root/metrics/prove) on a fresh `DemoNode` no longer start the engine or finalize the lazy genesis — only `produce_block()` seals. Credit after sealing throws `GenesisSealedError` (never a silent no-op). Regression tests in `test_demo_node.cpp` + the Go binding.
- **[x] C ABI error taxonomy (start).** `BlockchainerStatus` enum (`BLOCKCHAINER_OK`, `BLOCKCHAINER_ERR`, `BLOCKCHAINER_ERR_GENESIS_SEALED`, `BLOCKCHAINER_ERR_BAD_INPUT`). `blockchainer_node_credit` returns codes; all six bindings surface them (Go `Credit() error` + `ErrGenesisSealed`, Dart/Node/Swift/JVM throw, C++ typed exception). Extend to submit/produce next.

## Post-quantum tx signatures — Q1a (core) — DONE (2026)
- **[x] Hybrid Ed25519+ML-DSA-65 signatures in the tx path.** Extended tx envelope
  gated on the `version` byte: v1 = legacy Ed25519 (unchanged, all golden vectors
  intact); v2 carries a `sig_algo` byte (1=Ed25519, 2=hybrid). `sig_algo` is in
  `signing_bytes()` so it's committed to — a downgrade that strips the ML-DSA part
  fails, as does flipping the scheme. `verify_transaction_signature` dispatches on
  the effective algo and rejects unknown schemes. `crypto::hybrid_sig_*` wrap
  cryptolib's hybrid_sig. `ChainSpec.accepted_sig_algos` gates which schemes a
  chain accepts (empty = all; `{2}` = PQ-required), enforced in the ledger.
  Tested: `tests/test_hybrid_sig.cpp` (28/28 suite green). **Q1b (next): expose
  hybrid accounts through the DemoNode + C ABI + bindings, and cross-binding
  differential vectors (A2).**

## Post-quantum tx signatures — Q1b (end-to-end) — DONE (2026)
- **[x] Hybrid PQ path exposed through DemoNode + C ABI + Go binding.**
  `new_account_hybrid()` / `submit_transfer_hybrid()` in the core; C ABI
  `blockchainer_node_new_account_hybrid` (returns `[pub_len:4 BE][public][secret]`)
  + `blockchainer_node_submit_transfer_hybrid`; Go `NewAccountHybrid` /
  `SubmitTransferHybrid`. Account state machine relaxed to accept non-32-byte
  authors for non-Ed25519 schemes (sig verify enforces key validity first).
  `TestHybridTransferEndToEnd` (Go) drives create→credit→hybrid-transfer→block→
  proof; 28/28 C++ suite still green.
- **[x] A2 complete.** `NewAccountHybrid`/`SubmitTransferHybrid` exposed in Dart,
  Node, Swift, and JVM (all verified: 1984-byte PQ pubkeys, PQ transfer applied,
  proof verifies). Cross-binding differential (`scripts/hybrid_differential.sh`):
  Go emits a hybrid scenario, Dart replays it from the shared keys and reaches a
  byte-identical state root — PQ ledger state is deterministic across bindings
  even though ML-DSA signatures differ per run.
