# Aviary — Improvements & Deferred Work

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
- **[x] AssuranceProfile (one safety knob, committed in genesis) — DONE.**
  `AssuranceProfile` (`assurance.hpp`) is the umbrella that makes this "a safe
  library" rather than a parts bin: one named posture — `standard()` /
  `hardened()` / `fortress()` — fixes a *consistent* tier at every seam
  (signature algorithms, transport = mutual/ratcheted, consensus =
  stake-weighted/VRF, threshold custody, sealed mempool), so a deployment can't
  pair strong custody with a weak transport. `apply_to(GenesisSpec)` stamps the
  profile id AND the mandated `accepted_sig_algos`, and the id is **committed
  into the genesis hash** (new `GenesisSpec::assurance_profile` field) — so every
  node provably runs the same posture, and one can't silently join with a weaker
  config (the same suite-id-in-envelope discipline cryptolib uses).
  `from_genesis` reconstructs the profile AND rejects an inconsistent spec (a
  strong level whose accepted-signature set permits weak schemes) or an unknown
  id. `test_assurance.cpp`: tier mapping per level, the posture changing the
  genesis hash, encode/decode round-trip, and rejection of inconsistent /
  unknown profiles. GenesisSpec codec + fuzz totality still hold; golden vectors
  unaffected. **Remaining:** enforce the profile's transport/consensus/custody
  tiers automatically when a node boots from genesis (today the accessors report
  the tier; the node still wires the matching classes by hand).
- **[x] SealedMempool (anti-MEV encrypted mempool) — DONE.** `p2p::SealedMempool`
  (`sealed_mempool.hpp`) realises the `fortress` profile's sealed-mempool tier:
  a submitter seals the encoded tx to the block proposer's hybrid-KEM key
  (`crypto::pq_seal_to`) with a cleartext, AAD-bound `fee_hint`. The pool orders
  by that hint **without decrypting**, so a front-runner watching the mempool
  sees only ciphertext + a fee — never which contract or amounts — and cannot
  sandwich or re-order. `reveal(proposer_kem_secret, max)` opens the top-k at
  build time; entries that don't decrypt (not for us / tampered), don't decode,
  or whose real fee ≠ the hint (buying priority with a lie) are silently
  dropped. Submission-only: committed blocks carry ordinary plaintext txs, so
  consensus/ledger/wire are untouched. `test_sealed_mempool.cpp`: ciphertext-only
  pool, fee-priority reveal, a non-proposer opening nothing, lying-hint and
  tampered-envelope rejection, codec round-trip; `SealedTx::decode` in the fuzz
  sweep. ASan-clean. **Remaining:** per-height/epoch proposer key rotation and
  wiring `reveal` into `VrfPosEngine::propose` under the fortress profile.
- **[x] ProfiledValidator (assurance profile → node boot) — DONE.**
  `node::ProfiledValidator` (`profiled_validator.hpp`) is the integration seam
  that turns the `AssuranceProfile` from a *descriptor* into an *enforced* posture:
  `boot(genesis, me, bls_key, [vrf_secret, vrf_pubkeys])` reads the profile
  committed in the genesis, validates the genesis against it (rejecting an
  inconsistent or unknown posture), builds a ledger carrying the profile's
  signature-scheme gate, and wires the consensus engine the posture mandates —
  stake-weighted `PosEngine` for `standard`, grind-proof `VrfPosEngine` for
  `hardened`/`fortress` (which additionally require the node's VRF material, or
  boot throws). It hides *which* engine backs it: `propose`/`seal`/`submit` are
  uniform (both PoS families share the seal + the `ConsensusEngine` seam), and it
  reports the `threshold_custody`/`sealed_mempool` expectations. `test_profiled_
  validator.cpp` boots real validator sets from ONE profiled genesis at each
  posture and drives them to commit (standard→PoS one height, hardened→VRF three
  heights, all agreeing on the tip), asserts the signature gate reaches the state
  layer, and rejects inconsistent / unknown / VRF-without-keys genesises.
  Deterministic (fixed-seed VRF keys), ASan-clean.
  **Transport-tier wiring DONE:** `run_tcp_bft_node`'s `SecureConfig` now carries
  a `TransportTier` — `kMutual` (the SIGMA `SecureSession`) or `kRatcheted` (the
  `ArmoredChannel` PQ Double Ratchet, per-message FS + post-compromise healing).
  The reactor grew the ratchet path (Conn holds `chan` XOR `sess`; a per-frame
  1-byte control tag carries a keepalive so the initiator can prime the
  responder's sending chain — a ratchet responder can't speak until it decrypts
  once, which would otherwise deadlock a proposer that dialed in). `secure_config_
  from_records(tier, identity, prekey, self, records)` is the boot bridge: a node
  maps `profile.transport()` → tier and hands its address book. `test_tcp_
  transport.cpp` runs BFT over the ratcheted tier AND builds a committing mesh
  straight from hardened-profile peer records; ASan/UBSan + TSan clean, 8/8
  stable. **Remaining:** wire the sealed mempool into the boot path (blocked on
  the epoch-proposer-key design), and epoch/validator-set changes.
- **[x] RemoteGuard (OPAQUE admin gate) — DONE.** `node::RemoteGuard`
  (`remote_guard.hpp`) gates a node's privileged operations (remote key-store
  unlock, config) with OPAQUE (draft-irtf-cfrg-opaque, OPAQUE-3DH): the operator
  password NEVER leaves the client and the node stores NO password-equivalent —
  only an OPAQUE `record` useless to a stolen-disk attacker. A correct login
  mutually authenticates and agrees a session key; a wrong password fails without
  the node learning it. Capability pattern: at enrolment the admin seals a secret
  (a key-store pass, an admin token) under the OPAQUE **export_key** — a key the
  password derives client-side that the node can't compute — and the node stores
  that opaque blob, releasing it only on a successful login for the client to
  open. So the node holds an admin capability it cannot itself read, unlockable
  only by the right password. Adds `crypto::` OPAQUE wrappers (registration +
  3DH login) + `oprf_derive_keypair`. `test_remote_guard.cpp`: enrol → correct
  login retrieves the capability + matching session keys; wrong password can't
  finish; a tampered KE3 releases nothing; the stored record contains no password
  bytes; the node can't open the capability without the derived key; unknown
  credential rejected. ASan-clean. **Remaining:** run the OPAQUE messages over an
  `ArmoredChannel` and expose it through the C ABI + bindings.
- **[~] CredentialGate (BBS anonymous membership) — DESIGN DONE, impl staged.**
  Design doc: [`design/CREDENTIAL_GATE.md`](design/CREDENTIAL_GATE.md). A
  permissioned chain admits submitters / gossip peers by a zero-knowledge proof
  of holding an issuer-signed membership credential (BBS selective disclosure),
  disclosing only a required attribute (`tier ≥ k`) and the credential expiry.
  Policy (issuer key + schema + thresholds) committed into the genesis hash like
  `AssuranceProfile`. The doc resolves the core tension — BBS proofs are
  *unlinkable*, which breaks Sybil resistance / revocation / slashing — via a
  per-epoch verifiable pseudonym. **Audit finding (recorded in the doc):**
  cryptolib exposes only *bare* BBS (no pseudonym / blind-commit), so this stages
  into (1) shippable-now BBS wrappers + *unlinkable* Option-A admission
  (Sybil-weak but safe), and (2) the per-epoch pseudonym for real Sybil
  resistance, **blocked on an upstream cryptolib BBS-pseudonym primitive** — not
  a home-grown ZK construction (the project never reimplements crypto). Validator
  anonymity deferred behind slashing/epoch machinery. **BBS-pseudonym request
  FILED:** `docs/CRYPTOLIB_BBS_PSEUDONYM_REQUIREMENTS.md` (§1 per-verifier
  linkability/pseudonyms = the Option-B blocker, §2 blind issuance = the
  malicious-issuer fix). **DONE — bare-BBS wrappers + Option A:** `crypto::bbs_*`
  seam (keygen/sk_to_pk/sign/verify/proof_gen/proof_verify over cryptolib BBS,
  BLS12-381-SHA-256; `test_bbs.cpp`) + `consensus::CredentialGate` unlinkable
  admission (`credential_gate.{hpp,cpp}`): `CredentialPolicy`
  (issuer pk + schema + min_tier, canonical encode/decode_all/`hash()` ready to
  commit in genesis), `issue_credential` (trusted-issuer model, §10.2),
  `present`/`admit` — a holder proves in ZK it holds an issuer-signed credential
  with `tier ≥ min_tier` and unexpired `not_after`, disclosing nothing else;
  presentation bound to (chain_id, epoch, scope, challenge) for anti-replay;
  bulk issuer-epoch revocation. `test_credential_gate.cpp` (18 checks incl.
  unlinkability, context-binding, expiry, tier boundary, revocation,
  wrong-issuer, malformed-is-quiet-false); `CredentialPolicy::decode_all` in the
  fuzz totality sweep; ASan/UBSan clean; 58 C++ suites. **Surfaced through the C
  ABI + ALL FIVE bindings:** `aviary_bbs_keygen` (issuer key) +
  `_credential_policy_encode`/`_policy_hash` + `_credential_issue` +
  `_credential_present` + opaque `AviaryCredentialGate`
  (`_new`/`_revoke_epoch`/`_admit`/`_free`); credential blob
  `[sig_len:4][sig]([attr_count:4]([attr_len:4][attr])×)`. Go (`credential_gate.go`
  + `TestCredentialGate`), npm (`CredentialPolicy`/`CredentialGate` classes +
  `credential_gate.test.js`), JVM (Panama inner classes + Main check), Swift
  (`CredentialPolicy` struct + `CredentialGate` class + demo), Dart
  (`makeCredentialPolicy`/`makeCredentialGate` + classes + smoke) — each drives
  issue→present→admit + the full reject set and asserts unlinkability (two
  presentations of one credential differ yet both admit). **Mempool submission gate
  DONE:** `Mempool::set_credential_gate(const consensus::CredentialGate*)` +
  gated `add(tx, chain_id, epoch, challenge, presentation, now)` overload →
  `AddResult::kNotAdmitted` unless the presentation admits under scope "submit"
  (docs/design/CREDENTIAL_GATE.md §6). The plain `add(tx, chain_id)` stays the
  ungated TRUSTED path; an ungated pool ignores the presentation (permissionless
  unaffected); the gate is a *local* policy so `now` is caller-supplied and the
  mempool stays clock-free. `test_mempool.cpp` gated section (no presentation →
  kNotAdmitted, valid submit presentation → kAdded, wrong-epoch presentation →
  kNotAdmitted, trusted plain-add still works, ungated pool admits); ASan-clean.
  **Gossip peer-admission gate DONE:** the analog on the mesh —
  `GossipOverlay::set_credential_gate(const consensus::CredentialGate*)` +
  `admit_peer(peer, epoch, challenge, presentation, now)` → adds the peer only if
  the presentation admits under scope **"gossip"** (else the peer stays out and
  it returns false); the plain `add_peer` stays the ungated trusted path
  (genesis-known peers); ungated overlay admits everyone. `test_gossip.cpp`
  section proves it, including that a **"submit"-scope proof does NOT admit a
  gossip peer** (scope binding). ASan-clean. **Admission handshake over the wire
  DONE (2 increments):** (1) the pure protocol — `crypto::random_bytes` seam,
  `wire::kAdmissionChallenge`/`kAdmissionPresentation` (+ fuzz), deterministic
  `consensus::membership_epoch(height, epoch_length)` (§10.5), and
  `p2p::{make_admission_challenge, answer_admission_challenge, admission_granted}`
  (challenge→present→admit, transport-agnostic wire bytes); `test_admission.cpp`
  (unlinkability, cross-connection replay refused, epoch binding, wrong-issuer,
  garbage-is-quiet-false). (2) over a real socket —
  `p2p::admit_peer_over_socket` (verifier) / `present_over_socket` (prover), a
  blocking length-framed round, deliberately SEPARATE from the consensus reactor
  (`run_tcp_bft_node` already mutually-authenticates *validators* by sig key;
  anonymous *validators* are out of v1 scope per §3.4 — membership is for
  non-validator gossip / submitter peers). `test_admission_socket.cpp` drives it
  over a socketpair (verifier + prover threads): honest member admitted,
  unlinkable re-admit on a fresh connection, forged credential refused, garbage
  responder refused, silent peer times out to a refusal. ASan + **TSan**-clean.
  **Policy committed into the genesis hash DONE (§10.4 resolved: orthogonal).**
  `GenesisSpec::membership_policy` (canonical `CredentialPolicy` bytes; empty =
  permissionless) rides the genesis hash beside `assurance_profile` — the two
  axes are independent (a chain may be any posture × permissioned/permissionless).
  `CredentialPolicy::commit_to(GenesisSpec&)` / `from_genesis(...)` mirror
  `AssuranceProfile::apply_to`/`from_genesis`; core stays decoupled (stores bytes,
  the consensus layer derives the type). `test_genesis.cpp`: committing changes
  chain identity, round-trips hash-stable, reconstructs the exact policy, builds a
  working gate from genesis, and proves orthogonality (fortress+permissionless,
  standard+permissioned). Go + Dart golden vectors still replay (genesis encoding
  changed; tx/block/proof vectors unaffected); ASan-clean.
- **Option B — cryptolib primitive shipped; crypto seam DONE (Increment 1).** The
  filed requests landed (`Crdzbird/CryptoLib#3`/#4,
  `draft-irtf-cfrg-bbs-per-verifier-linkability-02`, pseudonyms **coupled with
  blind issuance** — the seed is committed blindly so the issuer never learns it).
  Relinked against the rebuilt cryptolib and wrapped all 9 new functions in
  `crypto::bbs_*`: `bbs_commit_with_nym` / `bbs_blind_sign_with_nym` /
  `bbs_finalize_nym_secrets` / `bbs_calculate_pseudonym` /
  `bbs_proof_gen_with_pseudonym` / `bbs_proof_verify_with_pseudonym` + standalone
  `bbs_blind_commit` / `bbs_blind_sign` / `bbs_verify_blind_sign`.
  `test_bbs_pseudonym.cpp` proves the blind issuance → finalize → pseudonym →
  proof → verify round-trip, plus the security invariants: **same context → same
  pseudonym** (Sybil-linkable within a scope), **different context → unlinkable**,
  a fresh seed → a different pseudonym, wrong-context / tampered-nym / wrong-blind
  all fail closed. **Gotcha (documented in the seam):** nym / entropy scalars must
  be canonical (< the BLS12-381 order r) — cryptolib does not reduce them, so a
  non-canonical scalar silently breaks the round-trip; the round-trip flow itself
  is byte-exact against cryptolib's own reference vectors. 61 C++ suites, ASan-
  clean, Go binding still green against the new lib. **Canonical-scalar helper
  wired (`Crdzbird/CryptoLib#5` shipped):** `crypto::bbs_hash_to_scalar(msg, dst)`
  + `bbs_random_scalar()` wrap the two new cryptolib fns; the test now derives the
  nym seed the clean way — `nym_seed = bbs_hash_to_scalar(member_secret, dst)`
  (deterministic, canonical, so the same member → the same per-context pseudonym)
  and takes issuer entropy from `bbs_random_scalar()` — no more high-bit clamping.
- **Option B — pseudonymous CredentialGate DONE (Increment 2).**
  `CredentialPolicy.mode` (kUnlinkable / kPseudonymous, committed in the genesis
  hash). Blind issuance as a 3-step protocol — `membership_commit` (holder derives
  `nym_seed = bbs_hash_to_scalar(member_secret)` and blind-commits it, the issuer
  never sees it) → `blind_issue` (issuer blind-signs {tier, not_after,
  issuer_epoch} + fresh entropy) → `finalize_membership` → `PseudonymousCredential`.
  `CredentialGate::present_pseudonymous` / `admit_pseudonymous`: `admit` returns
  the per-(chain, epoch, scope) **pseudonym** (std::nullopt on reject), stable
  across a member's presentations in that context (linkability domain = chain‖
  epoch‖scope, distinct from the challenge-bound `ph`). `revoke_nym` (targeted,
  §6.2), `note_admission` (Sybil cap §6.3 — first-vs-duplicate per epoch+scope),
  plus the Option-A issuer-epoch / expiry / tier checks. `test_credential_gate_
  pseudonym.cpp` (24 checks): same member+epoch+scope → SAME nym across fresh
  challenges (Sybil-linkable), different epoch/scope → unlinkable, different member
  → different nym, anti-replay, expiry, tier, targeted + bulk revocation,
  forged-issuer / malformed fail closed. 62 C++ suites, ASan-clean; Go binding
  still green (policy `mode` byte round-trips as default 0). Option A stays the
  unlinkable mode; the policy selects. **Surfaced through the C ABI + ALL 5
  BINDINGS:** `aviary_credential_policy_encode_ex(…, mode)` +
  `_membership_commit` / `_blind_issue` / `_finalize_membership` (3-step blind
  issuance; the membership-commit blob's first length-prefixed field is the
  commitment sent to the issuer) + `_credential_present_pseudonymous` +
  `_credential_gate_admit_pseudonymous` (returns the 48-byte nym, empty buffer on
  reject) + `_revoke_nym` / `_note_admission`. Go / npm / JVM / Swift / Dart each
  gain the pseudonymous flow + a test that drives blind issuance → per-(epoch,
  scope) pseudonym determinism → Sybil cap → cross-epoch unlinkability →
  revocation → the reject set. GOTCHA: the Swift demo lazy-bind-crashed because
  the vendored `CryptoLibC.xcframework` (macOS slice) predated the new cryptolib
  BBS symbols — fixed by rebuilding cryptolib's macOS slice
  (`scripts/ios/build_all.sh macosx`) and re-vendoring it into
  `cryptolib_flutter/{ios,macos}`; then the dynamic aviary xcframework
  rebuild picked it up. **The entire CredentialGate feature (Option A + B) is now
  end-to-end across all five bindings.**
- **[x] Composable chains — ChainBlueprint + ComponentRegistry + ChainAssembler,
  through the C ABI + ALL FIVE bindings — DONE.** A chain is a declared
  MIXTURE of pluggable components (consensus engine, state model, proposer
  policy) named by stable ids, with custom subclasses of the seams joining by
  registration; the mixture is committed into the genesis hash (chain identity),
  and an unknown id / seeding-mismatched state / posture-forbidden engine is
  rejected at assembly (fail-closed). `ChainBlueprint` (encode/decode/hash,
  `commit_to`/`from_genesis`; `GenesisSpec.chain_blueprint`, orthogonal to
  posture + membership), `ComponentRegistry` (built-ins bft/pow/pos/vrf-pos,
  account/utxo, stake-weighted/round-robin; `*_ids()` discovery; engines declare
  grind-proof), `ChainAssembler::assemble` → a generic `ConsensusEngine`.
  `test_chain_blueprint.cpp` (32 checks) incl. a custom engine subclass booted
  through the mixture. C ABI: `aviary_bls_keygen`, `_blueprint_encode/
  _hash`, `_registry_ids(kind)`, a minimal `_genesis_build` + `_genesis_commit_
  blueprint/_hash`, and an opaque `AviaryChain` (`_assemble` NULL on
  reject, `_propose`/`_seal`/`_mine`/`_submit`) — the uniform propose →
  (seal|mine) → submit flow. Go / npm / JVM / Swift / Dart each: discovery,
  Blueprint, buildGenesis, commitBlueprint (hash changes), assembleChain, PoS +
  PoW commit a block, and the three fail-closed rejects. Custom components stay
  in-process C++ subclasses (cross-language subclassing would need FFI callbacks
  — a follow-up); every language can discover and declare mixtures naming them.
  63 C++ suites, ASan-clean, Go golden vectors replay.
- **[x] Foreign components — engines / proposers implemented in ANY binding
  language — DONE** (commits d3ba92e, c712a63). The follow-up above, closed:
  a mixture component no longer has to be C++. The consensus pipeline stays
  native (`ForeignEngine : ConsensusEngine` proposes and applies through the
  Ledger, so atomicity + canonical encoding are never delegated); foreign code
  supplies only what genuinely varies — the JUSTIFICATION policy (justify a
  proposed block / verify a justification) and the proposer (leader-election)
  policy — as C callbacks with an opaque `ctx`: `AviaryJustifyFn`
  (writes into a caller buffer, cap `AVIARY_MAX_JUSTIFICATION`=4096),
  `AviaryVerifyFn`, `AviaryProposerFn`, registered via
  `aviary_registry_add_engine/_add_proposer`; `aviary_chain_seal`
  dispatches to the foreign justify. A throwing / refusing callback is a failed
  verification, never a crash. Go (cgo `//export` trampolines + `cgo.Handle`),
  npm (`koffi.register`), JVM (Panama upcall stubs), Swift (`@convention(c)` +
  `Unmanaged` ctx), Dart (`NativeCallable.isolateLocal`) each register an
  engine + a proposer and prove: discovery by id, seal → their justify, a
  tampered justification refused with the ledger untouched, a valid one commits,
  call counters confirm the boundary crossing, PoS with a foreign proposer
  commits. **Contract (documented, unenforceable by the registry):** callbacks
  run synchronously inside consensus, MUST be deterministic (no clock, no
  randomness) and must not re-enter the chain. 63 C++ suites, ASan-clean.
- **[x] Noise XX transport tier — standards-based channel + PQ identity proof
  — DONE** (commit e0702a7). Consumes cryptolib's newly exposed
  `cryptolib_noise_*` C ABI as `TransportTier::kNoiseXX`, next to the SIGMA
  `SecureSession` (kMutual) and the ratcheted `ArmoredChannel`. `crypto::Noise`
  wraps the raw Noise_XX_25519_ChaChaPoly_SHA256 state (+ `x25519_keygen`);
  `p2p::NoiseSession` runs the 3-message XX handshake (fixed version‖role hello
  payloads, checked fail-closed) and then exchanges one identity proof per side:
  the peer's HYBRID Ed25519+ML-DSA-65 key signs `kNoiseBinding ‖ role ‖ final
  handshake hash`. The hash commits every handshake byte, so a MITM's two
  sessions have two hashes and an honest proof never verifies in the other one
  — authentication survives a quantum adversary even though the channel's
  confidentiality is classical X25519 (documented: pick kMutual/kRatcheted for
  PQ confidentiality; kNoiseXX is for interoperating with any Noise stack).
  `encrypt/decrypt` refuse until the peer's proof verified. Reactor: dialer
  msg0 → msg1 → (msg2 ‖ proof) → peer proof + pin; accepter msg0 → msg1, msg2 →
  proof, peer proof + allow-list; no consensus byte flows before the proof step.
  The X25519 static is fresh per boot unless configured (the identity is the sig
  key), so no peer key is needed up front beyond the existing sig allow-list;
  optional shared prologue (e.g. genesis hash). `test_noise_session.cpp`
  (round trip, fail-closed ordering, tamper/reorder/handshake-tamper/prologue
  rejects, forged proof, and a full MITM holding both legs' Noise keys who
  cannot relay Bob's proof into Alice's session); `test_tcp_transport.cpp`:
  4-validator BFT commits over Noise XX, an unlisted identity is excluded while
  the quorum commits. 64 suites; ASan/UBSan + TSan clean. Not on the C ABI
  (no transport tier is — bindings stay 1-1).
- **[x] Subchains — a block that anchors a whole blockchain — v1 DONE**
  (design `docs/design/SUBCHAINS.md`, accepted with recommendations; commits
  2b892c6, daeb86f, 5c94d93, f6f11de, 98fe683, 54cd82d). A parent block
  carries ANCHORS: a child chain's consecutive finalized headers + commit
  certificates; every parent validator verifies each with the light-client
  rule (`LightClient::accept`, unchanged) under the child's FIXED validator set
  registered at the parent's genesis; the child's tip is committed in header v3's
  `subchain_root` (beside `anchor_root`), version-gated so v1/v2 encodings and
  golden vectors are byte-identical. `SubchainPolicy` (genesis-committed, parent)
  + `ParentBinding` (genesis-committed, child; a child cannot be re-parented) —
  registration by the child's genesis HEADER, because the genesis hash would be
  circular with the binding. `SubchainRegistry` lives in the Ledger (state model
  stays pluggable); §6 order validates anchors before any tx work on a working
  copy, all-or-nothing; snapshot/restore carry child tips and refuse a registry
  that mismatches the tip. `SubchainProof` descends hop by hop (any depth, one
  verifier, no new crypto) to `verify_state_proof`. `ConsensusEngine::
  propose_anchored` on every built-in engine (any engine may be a PARENT);
  in-process relay `node::pending_anchor(s)` from a child's BlockStore;
  `RoundDriver/NetworkedNode::set_anchors`. **Pre-existing gap closed while
  building:** `Ledger::validate_block` is a side-effect-free dry run of the
  commit pipeline and `RoundDriver::valid` now runs it before voting — a
  proposal with an invalid tx or anchor can no longer be certified and then
  refused at commit (stranding the height). C ABI (15 calls incl.
  `aviary_chain_certify` for single-validator BFT chains) + Go / npm /
  JVM / Swift / Dart parity, each proving the same scenario. Tests: 3-level
  tree with real BFT chains, cross-chain proof of a grandchild balance from the
  root header, 12 tamper-each-link rejects, 17-row anchor rejection matrix,
  child equivocation inert, 4-validator TCP parent commits the same anchoring
  block on all nodes. **v1 limits (§8 of the design):** BFT children only
  (PoS/VRF here are leader-signed, not certificate-finalized), child validator
  sets fixed at registration (needs validator-set changes in `LightClient`
  first), static registration, the parent never executes children (a child
  supermajority can anchor a false state root — same limit as any light
  client). 65 suites; ASan/UBSan + TSan clean.
- **[x] Validator-set changes — announced rotation — DONE** (commits 5c04e78,
  e322fb3). A block may announce the set in force from the NEXT height
  (`ConsensusData.next_validators`); the announcing block is certified by the
  CURRENT set (>2/3 of the old set attests the new one), so the change is
  itself finalized; the following block commits the new set's hash while its
  embedded last commit is verified under the old set (`validate_consensus_data
  (current, certifying, …)`). Followed everywhere a set lives: `LightClient`
  (tracks the set in force next and the set that certified the tip; resume-
  mid-chain ctor takes both; a client that ignores a rotation or resumes with
  the wrong certifying set cannot continue), `BftEngine`, `RoundDriver` /
  `NetworkedNode`, `SubchainRegistry` (a child's rotation is followed through
  anchors and snapshots; `subchain_root` and proof hops now commit each
  child's current set hashes). **Governance = coordinated rotation:** operators
  schedule the agreed set; the next proposal announces it; `precommit` /
  `valid()` refuse an UNSCHEDULED change (governance at vote time); a
  certified rotation is FOLLOWED regardless (`RoundStateMachine::Config::
  finalizable`, split from `valid` — found by the TCP test, where the one
  node that had not scheduled the change would otherwise strand itself on the
  network's decision); a node's index is remapped and a node rotated out keeps
  following without voting. C ABI `aviary_chain_schedule_validators` /
  `_chain_validators` + five-binding parity (grow, certify under each set,
  rotate out, PoS refuses). Still open: staking / bonding economics and epoch
  schedules.
- **[x] Light-client OVERLAP (skipping) rule + bisection — DONE** (commits
  b689a20, eed5bf2). `LightClient::accept_skipping(h, cert, h_set)` jumps to
  a far header iff (1) h commits `hash(h_set)`, (2) cert is a >2/3 certificate
  under h_set (h's own rule), and (3) the signers who are also in the client's
  TRUSTED set hold strictly more than `trust_level` (default 1/3) of its power —
  with fewer than 1/3 Byzantine among validators already trusted, at least one
  honest one vouched. Integer-exact, no floats. `Result.insufficient_overlap`
  separates "bisect" from "refuse": tampering (wrong set, altered powers,
  tampered header/cert, wrong chain, stale cert, not ahead, over `max_skip`)
  is always a hard failure. `sync_to(target, fetch)` bisects iteratively with
  a bounded fetch budget and fails closed on a missing or mis-heighted
  checkpoint. **Trusting-period assumption is documented, not enforced**:
  aviary has no consensus clock, so operators bound `set_max_skip` to
  their unbonding period and re-bootstrap a client whose tip is older. C ABI
  `aviary_light_client_*` handle + five-binding parity, each proving a
  no-overlap jump is "thin" (never a refusal), bisection through the rotation
  header, and continuation under the new set. 67 suites, ASan-clean.
- **[x] ChainTree — declare a FAMILY of chains and build / drive it together —
  DONE** (commits 30c7c4e core, ff35221 C ABI + Go, + bindings). The capstone
  over composable chains + subchains: `ChainTreeSpec` is a recursive node spec
  (chain id, mixture blueprint — built-in or registered custom / foreign
  components — assurance level, funded accounts, validators with the secrets
  this process holds, anchoring bounds, children); canonical codec (fuzz-
  total) + `validate()` (unique ids, validators, children must be BFT, depth
  ≤ 8). `ChainTree::build` resolves the ordering nobody should do by hand:
  genesis bottom-up (a parent registers each child by its genesis HEADER, with
  the child's subtree depth), bindings top-down (each child commits its
  parent's FINAL hash), assembly through the registry (fail-closed: an unknown
  mixture or a posture-forbidden engine throws and nothing is built).
  `produce(chain, ts)` proposes with the node's pending child anchors,
  justifies in-process (BFT certificate from the locally held validators —
  refuses if they lack quorum; PoS/VRF seal; PoW mine), submits and archives,
  so blocks flow upward automatically; `prove_account` walks root → … → chain
  into a `SubchainProof`; `verify` checks it against the root's current header.
  Extension: subclass hooks `select_txs` / `on_finalized`, or name custom
  components in a node's blueprint. C ABI `aviary_chain_tree_*` (node
  encoder, spec error, build, ids/height/genesis/tip/pending, produce, prove,
  verify) with the hooks as C callbacks; Go / npm / JVM / Swift / Dart each
  declare the same family (PoS root ← 3-validator BFT child ← funded
  grandchild + sibling), get structural reasons for bad specs, produce upward,
  observe finalizations and supply transactions from the host language, and
  prove the grandchild's account from the root. Flutter example gained a
  chain-tree card; Apple xcframework + Android jniLibs regenerated. 68 suites.
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
  **[x] VRF-PoS (grind-proof election) — DONE.** `VrfPosEngine` (`vrf_pos.hpp`)
  is the hardened tier composed on cryptolib's ECVRF (RFC 9381), closing the
  reference engine's two structural gaps: its leader is a public function of the
  height, so it is *predictable arbitrarily far ahead* (targeted DoS) and
  grindable by anyone influencing the seed. Here eligibility is a VRF proof over
  `alpha = tag ‖ chain_id ‖ height ‖ attempt ‖ parent_hash` — unpredictable
  without the leader's secret, un-grindable (beta is a function, not a choice),
  publicly verifiable from the proof carried in a versioned `VrfPosData`, and
  pinned to the exact tip by `parent_hash` (no cross-branch replay). Threshold:
  `ticket(beta) mod total_power < oversample × power`, so stake weighting and
  zero-power exclusion carry over; bounded `attempt` retries preserve liveness
  when nobody is eligible. `test_vrf_pos.cpp`: multi-height chain agreement,
  deterministic re-query, stake weighting, and adversarial rejection of
  tampered / impostor / out-of-range / bad-seal blocks **and of a genuine proof
  that fails the threshold**. `VrfPosData` in the fuzz sweep; ASan-clean. NOTE:
  VRF keys are engine config today (index-aligned with the validator set) —
  committing them into `GenesisSpec` is the follow-up. **Remaining:** fork
  choice for crowded attempts (linear ledger arbitrates today), VRF keys in
  genesis, epoch key rotation.
- **[~] PoW reference engine — DONE.** `PowEngine`
  (`pow_engine.hpp`) implements `ConsensusEngine` as a second consensus family: a
  block is justified by *work* — its header hash must be <= a difficulty target,
  with the nonce carried in `consensus_data` (so it is committed by the hash).
  `propose` yields an unsolved candidate, `mine` searches nonces, `submit`
  re-verifies the embedded work (target + hash<=target) then commits.
  `test_pow.cpp`: mine+submit, insufficient-work + wrong-target rejection, driving
  a chain through a `ConsensusEngine&`, PowData fuzzed. It is a *reference* engine:
  linear extension. **Difficulty retargeting DONE:** `PowEngine::retarget(current, actual, desired)` scales the 256-bit target by the clamped ([1/4,4]×) ratio of actual to desired block time (slower→easier, faster→harder, saturating at all-ones), via 256×64/64 big-integer math; `set_target` installs it. `test_pow.cpp`. **Remaining:** a windowed (multi-block) retarget + the full fork-choice weight = accumulated work.
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
  (`SubmitTransferFee`/`submitTransferFee`) via `aviary_node_submit_transfer_fee`.
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

- **[x] Custodial key management (S3) — DONE.** `aviary::custody`
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
  now exposed through the C ABI (`aviary_keystore_*`) and wrapped in every
  binding (Go/Dart/npm/Swift/JVM) as a `KeyStore` class — custodial signing is
  `reveal(address) → submitTransfer(secret, …)`, verified end-to-end on all four
  runtime bindings (`alice 750 / bob 250`, wrong-passphrase rejected) + a durable
  Go test (`TestKeyStoreCustodialSigning`). **MolecularVault upgrade (cryptolib
  3.x):** new entries are sealed by `Vault::seal_max` — cryptolib's MolecularVault
  cascade (XChaCha20-Poly1305 ∘ AES-256-GCM-SIV) under a key-committing outer
  layer, so BOTH ciphers must break before a signing secret at rest leaks. The
  store format is v2 with a per-entry seal version: legacy v1 stores (committing
  AEAD) still load and open, and re-serialising migrates the container while
  preserving each entry's true seal version. New portable backup:
  `KeyStore::export_account(addr, passphrase[, ops, mem])` seals {pubkey, secret}
  via MolecularVault's Argon2id passphrase mode (0,0 = SENSITIVE preset) with a
  fixed domain AAD, and `import_account` reseals it under the destination store's
  master — an account moves between stores with only a passphrase. Tamper /
  wrong-passphrase / wrong-AAD all fail closed; v1-compat + migration tested.
  **Threshold custody DONE (cryptolib FROST):** `custody::ThresholdAccount` /
  `SigningCeremony` (`threshold.hpp`) — t-of-n accounts via FROST(Ed25519,
  RFC 9591) where the account secret **never exists**: dealt shares sign in a
  two-round ceremony and the aggregate is a standard 64-byte Ed25519 signature,
  so a threshold-signed tx verifies through the untouched `kEd25519` path (the
  chain can't tell). A lying cosigner is *identified* (`verify_share`), not
  just rejected; shares seal at rest via MolecularVault with the AAD binding
  group + slot (`ShareCard::seal/unseal`, envelope prefix reproduces the AAD).
  `test_threshold.cpp`: 2-of-3 signs a transfer the Ledger commits, t-1 fails,
  corrupt cosigner named, cross-roster share never aggregates, sealed round-trip
  + rebind rejection; ShareCard::decode in the fuzz totality sweep.
  **Remaining:** zeroize master/secret buffers on drop; an optional `DemoNode`
  convenience that signs by address; expose export/import + threshold through
  the C ABI + bindings; distributed keygen (the dealer is trusted at creation).
  **Portable account tiers DONE (cryptolib Suite/PQ):** `custody::PortableAccount`
  is one export value {public_key, secret} with three fail-closed, domain-AAD-
  bound tiers: **passphrase** (MolecularVault + Argon2id), **recipient-bound**
  (`crypto::pq_seal_to` — encapsulate to a device's hybrid-KEM key; only that
  device opens, no shared secret to phish), and **signed-recipient** /
  Fortress-grade (`pq_seal_signed_to` — recipient-bound AND the receiver verifies
  the owner's hybrid signature). `KeyStore::extract`/`absorb` move an account
  between stores in any tier, resealed under the destination master.
  `test_portable.cpp` covers all three round-trips + wrong-passphrase / wrong-
  device / wrong-signer / tamper failing closed + a cross-store move.
- **[x] PQ / hybrid signatures in the tx path — DONE (three families).** The v2
  envelope's committed `sig_algo` byte + `ChainSpec.accepted_sig_algos` gating
  now carry THREE schemes: classical Ed25519 (`kEd25519`), the lattice hybrid
  Ed25519+ML-DSA-65 (`kHybridEd25519MlDsa`, Q1), and **SLH-DSA-SHA2-128s
  (`kSlhDsa` = 4, FIPS 205)** — stateless hash-based signatures whose security
  rests on SHA-2 alone, the crypto-agility fallback if lattice assumptions fall.
  The 128"s" profile minimises the on-chain signature (7856 B, pk 32 B) at the
  cost of signing time — the right trade for transactions, signed once and
  stored forever. No format change: `kSlhDsa` rides the existing v2 envelope,
  anti-downgrade via the committed algo byte as before. `test_hybrid_sig.cpp`:
  round-trip, encode/decode, tamper/wrong-key/algo-flip/truncation all fail,
  ChainSpec gating (a hash-only chain rejects both other schemes), and an
  SLH-DSA-signed transfer committing through the Ledger. **Remaining:** expose
  SLH-DSA account creation in the language bindings (hybrid already is).
- **[M] Crypto seam as interfaces.** `Hasher`/`Signer`/`Verifier`/`Aggregator`
  are free functions, not the injectable interfaces of `ARCHITECTURE.md` §3.2.
  Make them interfaces if we need suite-by-name selection or fake hashers in tests.

## C ABI & language bindings

- **[x] Expose the Ledger + BftEngine through the C ABI — DONE.** `DemoNode` (full chain: ledger+BFT+mempool+observability) is exposed via an opaque handle (`aviary_node_*`: create/credit/new_account/submit_transfer/produce_block/height/balance/nonce/state_root/drain_events/metrics/prove_account) + interop address derivation. A high-level Dart binding (`aviary.dart`) and a **Flutter example app** (`examples/aviary_demo`) drive all of it. Remaining: multi-validator nodes, EVM/BTC tx-signing over the C ABI.
- **[M] Packaged, auto-loading binding distribution.** Bindings load
  `libaviary_c.dylib` by path (dev only). Mirror cryptolib's packaging: a
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

- **[~] Real socket transport + event loop — DONE (TCP reactor).**
  `p2p::run_tcp_bft_node` (`tcp_transport.hpp` / `src/p2p/tcp_transport.cpp`)
  drives a `NetworkedNode` over real sockets: a single-threaded `poll(2)` reactor
  (no locks — one thread owns the node) that listens on loopback, dials the
  topology (lower id dials higher, sends a 4-byte id hello), framed-reads
  messages (u32-BE length prefix) into `on_gossip`, sends the node's relays, and
  fires consensus timeouts against a steady clock. It waits for every peer link
  before `start()` so the proposer never broadcasts into the void, and buffers
  frames that race ahead of its own StartRound. `test_tcp_transport.cpp`: four
  validators, one thread each, own ledgers, commit the SAME block over a
  fully-connected loopback mesh — ASan/UBSan **and** TSan clean, ~0.36s, 10/10
  stable. **Remaining:** wrap each frame in the mutually-authenticated secure
  session — DONE. Pass a `SecureConfig` (this node's `SecureSession::Identity`
  + peer KEM/signing public keys) and every connection runs the SIGMA-style
  handshake (dialer = initiator, accepter = responder) before any consensus
  byte, then encrypts + integrity-protects every frame. The dialer pins the
  peer's authenticated signing key; the accepter maps the authenticated key to a
  validator id via an allow-list — an unknown/mis-pinned identity is dropped.
  `test_tcp_transport.cpp` runs BFT over the encrypted channel AND shows a
  mis-pinned validator excluded while the honest quorum still finalizes (needed
  a connection *grace deadline* so an absent peer can't stall a quorate net).
  ASan/UBSan + TSan clean. **Lifecycle DONE:** sockets are
  non-blocking; sends go through bounded per-connection outbound queues (4 MiB
  cap) flushed on POLLOUT — a peer that stops reading past the cap is dropped,
  not allowed to wedge the reactor. A dropped dialer-side link **re-dials with
  backoff** (initial dial and reconnect share one path), rerunning the id hello
  / secure handshake on the fresh socket. Because gossip never re-floods what it
  already relayed, a healed link gets a **replay of the node's per-height
  consensus log** (`NetworkedNode::replay_log`) — without it a healed partition
  stays stalled (neither side can quorum, and Tendermint can't change rounds
  without 2f+1 crossing the cut); receiver dedup makes replay idempotent, and a
  late joiner catches up the same way. Fault injection via `ChaosConfig` (sever
  a link after N frames): `test_tcp_transport.cpp` severs the 1-2 cut vertex of
  a line topology mid-consensus and all four validators still commit the same
  block after redial + replay. 10/10 stable; ASan/UBSan + TSan clean.
  **Remaining:** a QUIC option (deliberately deferred — needs an external
  protocol library, against the zero-dependency design).
- **[~] Multi-validator vote gossip — DONE (in-memory).** `p2p::NetworkedNode` (`networked_node.hpp`) assembles {Ledger, RoundDriver, GossipOverlay} into a runnable validator: consensus broadcasts become gossip originations (+ self-delivery, since the round driver counts its own votes on delivery), gossip deliveries are deduped, fed to the driver, and relayed onward; timeouts arm/fire. `test_networked_node.cpp` runs BFT over partial-mesh topologies and shows **relay tolerance** (line/ring/star — a validator reaches quorum via hops it isn't directly connected to, all commit the same block) and **partition tolerance** (a connected 3-of-4 majority finalizes; an isolated validator stalls at height 0). ASan-clean. Now also driven over **real TCP** by `run_tcp_bft_node` (see the socket-transport item above), and adversarially tested by `test_byzantine.cpp`: an **equivocating** round-0 proposer (two validly-signed conflicting blocks to different peers, then silent) — the honest quorum still agrees on ONE block (safety) and finalizes it (liveness); and a **gossip-spam** flood of undecodable garbage — honest nodes drop it, the seen-set dedups the re-flood, and consensus finishes unaffected. **Remaining:** none material for the mesh itself.
- **[~] Gossip overlay manager — CORE + HARDENING DONE.** `p2p::GossipOverlay` (`gossip.hpp`) is the mesh on top of the wire/secure-session transport: it dedups messages by content hash (BLAKE3) and floods a new one to peers except the sender, so a message reaches every connected node exactly once over any (partial) topology, loop-free. **Hardened** so an adversary can't exhaust memory or dominate the mesh: the seen-set is a **bounded FIFO window** (capacity-limited, deterministic/clock-free eviction — the core forbids wall-clock; the window must exceed in-flight message count), and peers are **scored** — the caller reports misbehavior via `penalize` (the `NetworkedNode` does so on an undecodable payload) and a peer past the ban threshold is dropped: no longer relayed to, its messages ignored. `test_gossip.cpp`: ring/mesh/clique propagation, bounded-window eviction, and peer scoring→ban→silence. `test_byzantine.cpp` shows a `NetworkedNode` banning a persistent garbage-flooder end to end. Transport-agnostic (returns relays to send). **Remaining:** message TTL + topic-scoped subscriptions, positive scoring / gradual decay.
- **[~] Peer discovery & address book — DONE (PEX).** `p2p::AddressBook` of
  `PeerRecord{id, host, port, kem_pk, sig_pk}` (`peer_book.hpp`) is the directory
  the transport reads: where to dial each validator and which keys authenticate
  it. A joining node need not hard-code the set — it starts from a seed and
  learns the rest by exchanging books (pull-based PEX). `merge` is **first-
  writer-wins**, so an attacker can't rebind a known identity via gossip, only
  introduce genuinely-new ids. Canonically serialized (ascending id; strict
  decode rejects reorder/dupes/trailing bytes — in the fuzz totality sweep).
  `test_peer_book.cpp`: codec round-trip + canonicality, first-writer-wins,
  PEX convergence over a sparse line (every node learns all peers in ≤ n rounds,
  byte-identical books), and building the transport `SecureConfig` from a
  converged book. **Signed records DONE:** every `PeerRecord` is now
  self-signed — the owner signs the domain-tagged (`kPeerRecord` = 0x07) hash of
  the canonical unsigned fields with the same hybrid signature key the secure
  session authenticates with. `merge` (the PEX trust boundary) verifies each
  incoming record and rejects forgeries outright: a tampered field, a missing
  signature, or a wrong-key signature all fail; first-writer-wins still protects
  known ids against even *validly signed* rebinds. `add` stays the trusted local
  path (self/seeds, unverified). Note the honest limit: a signature binds a
  record to *a* key, not an id to a key — id squatting for never-seen ids is
  only prevented by seeding from genesis/validator-set data. **Remaining:**
  liveness/expiry and eclipse-resistant peer selection.
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
- **[x] Armored channel (PQ ratchet, hardened transport tier) — DONE.**
  `p2p::ArmoredChannel` (`armored.hpp`) composes cryptolib's hybrid-KEM **Double
  Ratchet** (`crypto::Ratchet`) with a hybrid-signature identity handshake to
  give a channel that is confidential, mutually authenticated, post-quantum,
  forward-secret, **and** post-compromise-secure — a leaked message key exposes
  neither past nor future traffic, and a full state leak on a long-lived
  validator link heals as the ratchet turns. The raw ratchet authenticates only
  the responder (prekey ownership); the channel adds initiator authentication —
  the initiator signs a transcript binding the ratchet handshake, the
  responder's prekey, and both identity keys, so the responder learns *who*
  dialed it and pins that identity. The responder's prekey is sourced
  authentically from its **signed PEX record** — `PeerRecord` gained a `prekey`
  field covered by the record signature, so an attacker can't substitute a
  prekey to MITM (swapping it invalidates the signature). `test_armored.cpp`:
  many-turn bidirectional messaging, replay rejection, mutual-auth handshake,
  impostor-identity rejection, responder-auth-by-prekey (an impostor lacking the
  prekey secret can't open), tampered-frame rejection, and end-to-end
  composition straight from a verified `PeerRecord`. ASan/UBSan-clean.
  **Remaining:** wire `kRatcheted`/`kFortress` tiers into `run_tcp_bft_node`
  (the transport still uses the SIGMA `SecureSession`), and rekey-on-reconnect.
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
- **[M] No install/export for aviary itself.**
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
- **[x] C ABI error taxonomy (start).** `AviaryStatus` enum (`AVIARY_OK`, `AVIARY_ERR`, `AVIARY_ERR_GENESIS_SEALED`, `AVIARY_ERR_BAD_INPUT`). `aviary_node_credit` returns codes; all six bindings surface them (Go `Credit() error` + `ErrGenesisSealed`, Dart/Node/Swift/JVM throw, C++ typed exception). Extend to submit/produce next.

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
  `aviary_node_new_account_hybrid` (returns `[pub_len:4 BE][public][secret]`)
  + `aviary_node_submit_transfer_hybrid`; Go `NewAccountHybrid` /
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

## Safety classes through the bindings — DONE (2026)
- **[x] ThresholdAccount (FROST t-of-n) through the C ABI + all five bindings.**
  Ten `aviary_threshold_*` functions (create → per-share info blob
  `[id u16][gpk 32][pub 32]` → seal/unseal a share → round-1 `commit`
  `[nonces 64][commitment 66]` → opaque `Ceremony` handle → `sign` → attributable
  `verify_share` → `aggregate`). The group secret never exists on the ABI; the
  aggregate is a **standard 64-byte Ed25519 signature**, and each binding proves
  that with its *own* Ed25519 verifier under the group key — cryptolib (Go),
  `node:crypto` SPKI-wrap (npm), `java.security` (JVM), CryptoKit
  `Curve25519.Signing` (Swift), and the shared ABI attribution check (Dart).
- **[x] AssuranceProfile + SealedMempool through the C ABI + all five bindings.**
  `aviary_assurance_describe(level)` returns the posture descriptor blob
  `[transport u8][consensus u8][threshold_custody u8][sealed_mempool u8][algo_count u8][algos…]`;
  bindings expose it as an `AssuranceProfile` (ratcheted / vrf / thresholdCustody
  / sealedMempool + admitted sig algos) and assert the tiers are monotonic
  (standard → hardened → fortress only tighten) with unknown levels failing
  closed. `aviary_tx_encode(...)` canonically encodes a fee-bearing tx so
  a binding can seal one; `aviary_sealed_tx_seal` HPKE-seals it to a
  proposer's KEM key; `aviary_sealed_mempool_{new,add,size,reveal,free}`
  runs the anti-MEV pool. Every binding's smoke test seals txs to a proposer,
  reveals them all with the proposer's KEM secret, and confirms a **non-proposer
  key opens nothing** — the confidentiality guarantee. Go/npm/JVM/Swift/Dart all
  green; 56/56 C++ suite still passes.

## pub.dev release structure: three Dart packages over CipherBird - DONE (2026-10-10)
- **[x] cryptolib is now CipherBird.** The dependency repo is still checked out
  at `../cryptolib` (remote Crdzbird/CipherBird); the C header and `cryptolib_*`
  symbol prefix are unchanged. Its Flutter plugin is the `cipherbird` pub package
  (1.1.0) shipping `CipherBird.xcframework` and `libcipherbird.so` (SONAME still
  `libcryptolib_c.so`), and its pure Dart package is `cipherbird_dart`. Build
  scripts accept `CIPHERBIRD_ROOT` (falls back to `CRYPTOLIB_ROOT`, then the
  sibling checkout).
- **[x] `aviary_dart` restructured for publishing.** The 2152-line
  `aviary_ffi.dart` became one library (`lib/aviary.dart`) with 71
  part files under `src/{ffi,core,types,api,handles}`: feature surfaces as
  extensions on `final class Aviary`, value types as
  `abstract final class` with `const factory` redirects to private
  implementations, handle classes with private constructors, immutable
  `TreeNode` with `copyWith`. Every file is under 100 lines after `dart format`,
  no `else`, no inline comments. Bundles `native/darwin-arm64/
  {libaviary_c,libcipherbird}.dylib` resolved through the package root in
  `package_config.json`. 12 smoke tests pass on the bundled engine.
- **[x] Two Flutter variants with the same API.** `aviary_lite` is self
  contained: static merged `Aviary.xcframework` (aviary archives +
  CipherBird per-slice `libcryptolib_c_merged.a`, iOS device + simulator, macOS)
  and a self-contained stripped `libaviary_c.so` (arm64-v8a, x86_64) whose
  only NEEDED entries are libm/libdl/libc. `aviary` (renamed from
  `aviary_flutter`) depends on and re-exports `cipherbird`: dynamic
  `AviaryC.xcframework` linked with `-undefined dynamic_lookup` and a thin
  `.so`; the Dart loader and the Kotlin startup provider open `libcipherbird.so`
  before `libaviary_c.so` because of the SONAME mismatch. Both plugins are
  SPM-only ffiPlugins (`ios|macos/<name>/Package.swift`, binary target + a Swift
  target that imports `AviaryC`).
- **[x] Packaging rules learned.** pub only ships git-tracked files and honors
  the root `.gitignore`, so binaries are force-added and each package carries a
  `.pubignore` with `!` re-includes; path dependencies are not publishable, so
  `pubspec_overrides.yaml` (excluded from the archive) points `aviary_dart`
  at `../dart` for local work; archives are 5 MB (dart), 5 MB (full), 26 MB
  (lite, after per-platform slices and `llvm-strip`). pub flattens symlinks, so a
  versioned macOS framework arrives with `Versions/Current` as a real directory;
  Xcode warns "Couldn't resolve framework symlink" and builds anyway. Android
  plugin modules must set the Kotlin `jvmTarget` to 17 to match
  `compileOptions`. `flutter create --platforms=ios,android,macos .` scaffolds
  the example apps; their scaffolded widget tests were dropped.
- **[x] Verified.** Both example apps build for macOS and Android (lite APK ships
  only `libaviary_c.so`; full APK ships it plus `libcipherbird.so`); the
  full macOS app embeds `AviaryC.framework` and `CipherBird.framework` and
  runs. `examples/aviary_demo` and `examples/interop/app` now depend on
  `aviary`. `dart analyze` is clean for all three packages and both
  examples; `flutter test` passes for both plugins; `dart pub publish --dry-run`
  succeeds for all three with only the expected gitignore and overrides notes.
- Publish order: `aviary_dart`, then `aviary_lite`, then
  `aviary` (the Flutter packages pin `aviary_dart: ^0.1.0`).

## pub.dev polish: 160/160 pana score and the shortcut extensions - DONE (2026-10-10)
- **[x] pana 160/160 for `aviary_dart`** (`dart pub global activate pana`,
  `pana --no-warning .`). Fixes: descriptions trimmed to 60 to 180 characters,
  `example/aviary_dart_example.dart`, doc comments on every const factory
  redirect and a documented `const ForeignEngine()`, no angle brackets in doc
  comments, braces around every multi line `if` body (pana runs newer lints
  than the package's `lints` pin). `aviary_lite` and `aviary`
  score 140 locally only because the local run has to use a path dependency on
  `aviary_dart`; the published `^0.1.0` removes that warning. Each
  Flutter package exposes a documented entry point (`AviaryLite`,
  `AviaryShared`: `ensureInitialized`, `instance`, `isInitialized`) so
  dartdoc has API elements of its own to count.
- **[x] Shortcut extensions** under `lib/src/extensions/` (one per file):
  `HexBytes` (`hex`, `shortHex`, `equalsBytes`), `HexString` (`hexBytes`,
  `utf8Bytes`), `AccountTools` (`addressHex`, `isHybrid`), `NodeFunding`
  (`fundAccount`, `fundHybridAccount`), `NodeSending` (`send`, `sendTo` with
  the nonce read from the chain and the scheme following the account,
  `commit`), `NodeViews` (`events` decoded, `metricValues`,
  `verifyAccountProof`), `ChainBlocks` (`produceBlock` = propose, justify,
  submit), `ValidatorKeyPair` (`entry`, `treeValidator`), `AviaryQuick`
  (`newValidator`, `assembleSingleValidator` defaulting to bft over account),
  `ThresholdSigning` (`signWithShares` runs the whole ceremony),
  `TreeNodeBuilding` (`withChild`, `find`, `chainIds`), `ChainTreeRounds`
  (`produceRound`, children first). Covered by `test/extensions_smoke.dart`.
- GOTCHA: a genesis assembled without a committed blueprint is not a BFT
  chain (`certify` fails with "engine is not BFT"); commit
  `makeBlueprint(consensus: 'bft', stateModel: 'account')` explicitly.
- OPEN: `https://github.com/Crdzbird/aviary` answers 404 anonymously
  (private repository). pub.dev verifies the homepage, repository and
  documentation URLs with HEAD requests and withholds 10 points while they
  fail, so the repository must be public (or the URLs changed) before the
  packages can reach 160 on pub.dev.

## Rename: blockchainer is now Aviary - DONE (2026-10-10)
- **[x] Whole project renamed.** Mechanical, case preserving substitution of
  blockchainer, Blockchainer and BLOCKCHAINER with aviary, Aviary and AVIARY
  across 478 tracked text files (4592 occurrences) and 497 renamed paths. The
  C++ namespace is `aviary`, the C ABI prefix `aviary_*`, the header
  `bridge/aviary_c.h`, the CMake targets `aviary_core` and `aviary_c`, the
  options `AVIARY_*`, the native library `libaviary_c`, the env override
  `AVIARY_DYLIB`. Packages: pub `aviary_dart`, `aviary_lite`, `aviary`
  (Dart class `Aviary`, entry points `AviaryLite` and `AviaryShared`); npm
  `aviary-ledger` (plain `aviary` is taken on npm); Go module
  `github.com/Crdzbird/aviary/bridge/bindings/go`; JVM package `aviary`;
  Swift package `Aviary` with the `AviaryC` binary target; Android plugin
  `aviary-android`; Flutter demo `examples/aviary_demo`.
- Consensus bytes are unchanged: the golden vectors only carried the name in
  comments. The sealing and transport labels (`aviary.share.v1`,
  `aviary.portable-account.v1`, `aviary.account.export.v1`,
  `aviary.credential.v1`, `aviary-p2p-*-v1`, `aviary.remoteguard.*`) and the
  Prometheus metric names (`aviary_chain_height`, ...) did change, so
  envelopes and sessions produced before the rename do not open after it.
  Nothing was released before it.
- Every native artifact (bundled dylibs, both xcframeworks, both Android
  `.so` sets) was rebuilt because they carry the symbol names.
- The GitHub repository must be renamed to `aviary` by the owner (the Go
  module path and the pubspec URLs already point there; GitHub redirects the
  old name). The local checkout directory keeps its old name for now.

## Dart lifecycle, typed errors, worker isolates, Apple minimums - DONE (2026-10-10)
- **[x] Finalizers.** Every handle class extends a private `_NativeHandle`
  (`implements Finalizable`) that attaches a `NativeFinalizer` for its free
  symbol on construction; `free`/`dispose` detach and release once, a second
  call is a no-op, `isReleased` reports it. `ChainTree` keeps its explicit
  `free` (it also closes the two `NativeCallable`s and leaves the registry).
  `withNode`, `withKeyStore`, `withTree` release at scope end.
- **[x] Typed errors.** `AviaryStatus` mirrors the C ABI codes (ok, failure,
  genesisSealed, badInput); `AviaryException.status` defaults to `failure`
  for message only errors; `Node.credit` maps its status code.
- **[x] Worker isolate variants.** `sealShareAsync`, `unsealShareAsync`,
  `sealAccountPassphraseAsync`, `openAccountPassphraseAsync` run through
  `Isolate.run`; the worker loads the engine from the same explicit path the
  main instance used (`Aviary._path`) or the platform default. Buffer
  helpers moved to a private extension (`core/buffers.dart`) to keep the core
  class under 100 lines.
- **[x] Apple minimums** raised to iOS 15 / macOS 12 in both plugin
  manifests, the dynamic xcframework script (link flags and Info.plist) and
  the interop app, matching the CipherBird binary. Lite's static slices
  already follow CipherBird's toolchain.
- Tests: `test/lifecycle_smoke.dart` (double release, 200 undisposed nodes,
  typed status, scopes, async round trips, wrong passphrase).

## Typed events and a push stream - DONE (2026-10-10)
- **[x] C ABI `aviary_node_set_event_hook(node, fn, ctx)`** with
  `typedef void (*AviaryEventFn)(void* ctx, const char* json, size_t len)`.
  `DemoNode::set_event_hook` delivers each JSON object synchronously from the
  bus subscription; while a hook is set nothing is buffered for
  `drain_events`. The JSON now carries `block_id` on `block_finalized`, `txid`
  on `tx_removed`, and a new `block_rejected` kind. Only the Dart binding
  consumes the hook so far; Go, npm, JVM and Swift keep draining.
- **[x] Dart `ChainEvent`** is a sealed class with `BlockFinalized`,
  `BlockRejected`, `TxAdmitted`, `TxRemoved` and `UnknownEvent` (abstract
  final + const factory + private impl, one class per file under
  `types/events/`). `Node.events()` returns the typed list; `Node.eventStream`
  attaches a `NativeCallable` (isolateLocal, keepIsolateAlive false) through a
  `_NodeEventSink` registry keyed by handle address and feeds a broadcast
  `StreamController`; `Node.dispose` unhooks first, then frees, then closes.
  Listeners run on the next microtask, never inside the native callback.
- Housekeeping: the 13 typedef only `sigs_*` parts became 5 grouped files and
  the smallest same receiver extension files were merged, keeping
  `lib/aviary.dart` at 98 lines and every file under 100.
- Tests: `test/events_smoke.dart` (pull, push, order, exhaustive switch,
  lenient decoding). All native artifacts rebuilt.

## Persistence surface for the node - DONE (2026-10-10)
- **[x] `DemoNode` keeps a `BlockStore` archive** (every finalized block with
  its certificate) and the ordered genesis credits. `snapshot()` encodes
  `[chain_id u64][credits u32]([address 32][amount u128])*[archive bytes]`;
  `restore(bytes)` recreates the node (the validator identity is derived from
  the chain id, so nothing secret is stored), replays the credits, then
  `BftEngine::finalize`s every archived block in order so each certificate
  is verified and the engine ends in the exact state it had (last commit
  included), and clears the replayed events. The mempool is not persisted.
  `block_at(h)` and `archived_blocks()` read the archive.
- **[x] C ABI** `aviary_node_snapshot`, `aviary_node_restore` (NULL on a
  malformed blob), `aviary_node_block_at`. Dart: `NodeStorage` (`snapshot`,
  `blockAt`, `saveTo` with temp file + rename) and `AviaryNodeStorage`
  (`restoreNode` throwing `badInput`, `restoreNodeFrom` returning null for a
  missing file). `test/persistence_smoke.dart` and a C++ case in
  `test_demo_node.cpp` cover round trip, continued production, file round
  trip, garbage, missing block and empty snapshot. Native artifacts rebuilt.
- Not done here: the directory based `storage::save_chain`/`load_chain`
  (FileStore + `Ledger::snapshot`) stay C++ only; the blob approach lets each
  app choose its storage. Go, npm, JVM and Swift do not wrap the new calls
  yet.

## Networking surface: NetworkedNode and SecureSession over the C ABI - DONE (2026-10-10)
- **[x] Core:** `RoundDriver::set_transactions(txs)` (proposed at this height,
  validated by every validator, cleared by `advance()`) replaces the empty
  block the driver used to build; `NetworkedNode::set_transactions` forwards
  it and `NetworkedNode::advance()` rolls into the next height (clears the
  catch-up log, gossips the new StartRound(0) actions). New
  `test_networked_node` case: full mesh of 4, a transfer in block 1, advance,
  block 2, identical tips.
- **[x] C ABI:** `aviary_net_node_*` (`new` from genesis bytes + validator
  index or `AVIARY_NET_OBSERVER`, `add_peer`, `set_transactions` blob list,
  `start`/`on_gossip`/`on_timeout`/`advance` returning the OUTBOUND blob
  `[relays u32]([peer u32][len u32][bytes])*[timeouts u32]([step u8][round u32])*`,
  `committed`, `height`, `peer_banned`, `replay_log`, `tip`, `state_root`,
  `balance`, `prove_account`, `free`) and `aviary_secure_*` (`identity_generate`
  blob list [kem pub][kem sec][sig pub][sig sec], `initiate`, `accept`,
  `handshake_message`, `complete`, `open`, `peer_identity`, `encrypt`,
  `decrypt`, `free`; a `SecureBox{session, handshake}` holds the pending
  handshake message).
- **[x] Dart:** `AviaryNetwork` (`newNetNode`, `generateSecureIdentity`,
  `initiateSecureSession`, `acceptSecureSession`, `observer`), `NetNode` +
  `NetNodeQueries`, `SecureSession`, `ConsensusStep`, records `Relay`,
  `TimeoutRequest`, `Outbound`, `SecureIdentity`. Tests:
  `network_smoke.dart` (deterministic 4 validator line mesh plus an observer,
  relays through hops, advance to height 2, garbage payload, proof from a
  networked node, secure session handshake, both directions, tamper, wrong
  identity) and `network_tcp_smoke.dart` (two validators over a real loopback
  TCP socket with length prefixed frames encrypted by SecureSession, Dart
  Timers for consensus timeouts, two heights).
- Part budget: six small merges (proofs into canonical, node storage into
  nodes, light client api into sync, foreign engine type into foreign api,
  credentials sigs into tree sigs, pseudonymous gate into gate) kept
  `lib/aviary.dart` at 98 lines.
- GOTCHA: the proposer's `replay_log` is non empty right after `advance()`
  because its new proposal is logged immediately; check a follower for the
  "log restarts" property. A mesh harness must keep draining after the
  validators commit so observers receive their queued relays.
- Not done: Go, npm, JVM and Swift do not wrap the networking calls; the
  ratcheted (`ArmoredChannel`) and Noise XX tiers are not exposed; address
  book / PEX is not exposed.

## Key store backed signing - DONE (2026-10-10)
- **[x] Core:** `include/aviary/custody_signing.hpp` +
  `src/crypto/custody_signing.cpp`: `custody::sign_transfer(store, from, to,
  amount, nonce, fee, chain_id)` reveals the sealed secret inside the core,
  signs, and wipes it (`custody::wipe`, volatile stores). Scheme follows the
  stored public key (32 bytes Ed25519, longer hybrid Ed25519 + ML-DSA-65);
  zero fee gives the version 1 envelope, byte identical to a plain transfer.
  `DemoNode::submit(tx)` / `submit_encoded(bytes)` admit an already signed
  transaction (chain id checked); `DemoNode::chain_id()` accessor.
  `test_keyring` gained a case: ed25519, hybrid, fee, locked, unknown.
- **[x] C ABI:** `aviary_keystore_sign_transfer`, `aviary_keystore_public_key`,
  `aviary_keystore_has`, `aviary_keystore_addresses`, `aviary_keystore_lock`,
  `aviary_node_submit_tx`, `aviary_node_submit_transfer_keystore`.
- **[x] Dart:** `KeyStoreSigning` (`signTransfer`, `publicKey`, `has`,
  `addresses`, `lock`), `NodeTransfers.submitTx` and
  `submitTransferFromStore`, `NodeSending.sendFromStore` (auto nonce).
  `test/custody_signing_smoke.dart` covers ed25519 + hybrid + fee from the
  store, lock/unlock, unknown account, garbage, a reloaded store signing, and
  a custody signed transfer carried by a `NetNode` block. `Blueprint` moved
  into `api/mixtures.dart` to keep the part budget.
- GOTCHA: a C ABI declaration that mentions `AviaryKeyStore` must come after
  that typedef (the header is ordered by section).

## Native assets: build hook, Linux pipeline, Windows slot - DONE (2026-10-10)
- **[x] `aviary_dart` build hook** (`hook/build.dart`, deps `hooks ^2.2.0`,
  `code_assets ^2.1.0`, SDK `^3.10.0`): for Linux and Windows targets it
  declares `native/<os>-<arch>/libaviary_c.so|aviary_c.dll` as a
  `DynamicLoadingBundled` code asset named `aviary.dart` (the library URI, so
  `@Native` resolves it); quiet on Apple and Android (the Flutter plugins
  carry the engine there) and for the macOS pure Dart case (package root
  lookup); quiet when no binary exists for the target so builds never fail.
- **[x] Loader:** C ABI `aviary_library_path(out, cap)` (dladdr / Win32
  GetModuleHandleEx) returns the loaded module's path; the Dart loader calls
  it through `@Native` first and reopens the asset by path so
  `lookupFunction` keeps working, falling back to the previous strategies.
  `Aviary.loadedFrom` reports explicit / asset / process / bundled / system;
  `preload_smoke` asserts `asset` on Linux.
- **[x] Linux pipeline:** `scripts/linux/build_so.sh` (CipherBird merged
  static archive, `-Wl,--exclude-libs,ALL -Wl,-z,defs`, strip, into
  `native/linux-<arch>/`) and `.github/workflows/native-linux.yml`
  (ubuntu-24.04 + ubuntu-24.04-arm, builds CipherBird + aviary, runs every
  Dart smoke test through the hook, uploads `aviary-linux-<arch>`). The CI
  workflow's CipherBird default repo is now `Crdzbird/CipherBird`.
- **[x] Linux shipped.** Run 38063524511 of `native-linux.yml` built and
  tested on ubuntu-24.04 and ubuntu-24.04-arm: every Dart smoke test passed
  on Linux with `Aviary.loadedFrom == asset`, proving the hook path end to
  end. The two libraries are committed under `native/linux-x64/` (self
  contained, 21 MB x64 and 14 MB arm64 after strip) and `native/linux-arm64/`; `platforms:` lists
  linux. Two Linux only fixes came out of it: 46 sources relied on
  transitive standard headers that libstdc++ does not provide (`<algorithm>`
  and friends), and Ubuntu's OpenSSL 3.0 lacks AES-256-GCM-SIV, so the
  workflow builds CipherBird's from-source OpenSSL 3.6 (`CFLAGS=-fPIC`) and
  points pkg-config at it with a one line `.pc` file.
- **Windows:** the hook and loader support `native/windows-x64/aviary_c.dll`
  and the C ABI builds for `_WIN32`, but no binary ships: CipherBird has no
  Windows build of its dependencies (OpenSSL, liboqs, libsodium, blst,
  secp256k1, BLAKE3). That upstream port is the prerequisite.
- **Formatting fallout:** the SDK bump switched `dart format` to the tall
  style; typedef files are kept under 100 lines with the private aliases
  `_U8`, `_V`, `_BR` and single line typedefs, `trailing_commas: preserve` in
  `analysis_options.yaml`, helpers moved (`_decodeOutbound`,
  `_chainEventsFromJsonArray`, `_shareInfo`) and the two spare part slots used
  for `blueprint.dart` and `credential_gate_pseudonymous.dart`. Tests written
  this session were split with `test/support/{check,mesh,tcp}.dart`; eight
  older smoke tests already exceeded 100 lines before this session and were
  left as they are.

## package:test conversion, Dart CI, tag driven publishing - DONE (2026-10-10)
- **[x] Suites.** The 22 `test/*_smoke.dart` scripts became
  `test/*_test.dart` package:test suites (`test 1.33.0`): each former `main`
  body is one `test(...)`, the boolean helper is `check(bool, String)` because
  package:test owns `expect`, and `dart_test.yaml` sets `concurrency: 1`
  (one native engine, process wide registries) and `timeout: 2x`. `dart test`
  runs all 22 in about a minute on the host; the Native Linux workflow runs
  `dart test` too.
- **[x] CI:** `.github/workflows/dart.yml` runs `dart format
  --set-exit-if-changed`, `dart analyze --fatal-infos`, `dart test` and
  `pana --exit-code-threshold 0` for `aviary_dart` on macos-latest and
  ubuntu-latest (both bundled engines are in the repo, no native build
  needed), and `flutter analyze --fatal-infos` + `flutter test` for
  `aviary_lite` and `aviary` on macOS.
- **[x] Publishing:** `.github/workflows/publish.yml` publishes on tags
  `aviary_dart-v*`, `aviary_lite-v*`, `aviary-v*` with `id-token: write`
  (pub.dev GitHub Actions trust via `dart-lang/setup-dart`), checks the
  pubspec version against the tag, removes `pubspec_overrides.yaml`, and runs
  `dart`/`flutter pub publish --force`. The pub.dev side (enable automated
  publishing per package, repository `Crdzbird/aviary`, tag pattern
  `<package>-v{{version}}`) is the owner's one time setup.
- GOTCHA: package:test only discovers `*_test.dart`; `dart test` silently
  reports "No tests were found" otherwise.
- **[x] The original `CI` workflow is green again** (run 38077401151, about
  11 minutes): it needed `secp256k1` from Homebrew, blst built from source
  with its headers under `/opt/homebrew/include/blst/` (mirroring CipherBird's
  own workflow), the CipherBird default repository `Crdzbird/CipherBird`, and
  JDK 23 because the runner's Java 21 treats the foreign function API as a
  preview. It runs the 68 C++ suites plain and under ASan/UBSan, the Dart
  demos, the Go demo, the JVM demo and the Flutter demo analysis.

## Apple privacy manifests - DONE (2026-10-10)
- **[x]** `bridge/apple/PrivacyInfo.xcprivacy` (NSPrivacyTracking false, no
  domains, no collected data types, accessed API types: FileTimestamp C617.1
  and SystemBootTime 35F9.1). Symbol scan: `AviaryC` itself imports none of
  the required reason APIs (std::filesystem lives in libc++), the lite static
  archive and the CipherBird framework import `stat`, `fstat` (OpenSSL) and
  `sysctlbyname` (libsodium), so the declaration is conservative and
  identical everywhere. Copies as processed resources in the four Flutter
  plugin Swift targets and the `Aviary` Swift binding target; the dynamic
  xcframework script embeds it in each `AviaryC.framework` slice (iOS root,
  macOS `Versions/A/Resources`). The static lite xcframework cannot carry a
  manifest, so its plugin target's resource is the one that ships.

## Go, npm, JVM and Swift wrappers for events, persistence, custody and networking - DONE (2026-10-10)
- **[x] Go** (`events.go` + `events_export.go` with the cgo export pattern,
  `persistence.go`, `custody.go`, `net.go` with `splitBlobList`,
  `decodeOutbound`, `NetNode`, `SecureSession`, `SecureIdentity`); `Node`
  gained `handle`/`onEvent` fields and an unhooking `Free`. Tests in
  `extras_test.go` (persistence, custody, event hook, four node line mesh with
  observer plus secure session).
- **[x] npm** (`AviaryEventFn` proto with a `const uint8_t*` payload because
  koffi hands `const char*` to JS as a decoded string, `Node.onEvent`,
  `snapshot`/`blockAt`/`submitTx`/`submitTransferFromStore`, `restoreNode`,
  `KeyStore.signTransfer`/`publicKey`/`has`/`addresses`/`lock`, `NetNode`,
  `SecureSession`, `generateSecureIdentity`, `splitBlobList`; typings in
  `index.d.ts`). Tests `persistence`, `custody`, `events`, `network`.
- **[x] JVM** (handles for all new symbols, `Node.setEventHook` through a
  static upcall stub and `NODE_EVENTS` map, `snapshot`/`blockAt`/
  `submitTx`/`submitTransferFromStore`, `restoreNode`, KeyStore methods,
  records `Relay`/`TimeoutRequest`/`Outbound`/`SecureIdentity`, inner
  classes `NetNode` and `SecureSession`). `Main.java` exercises all of it.
- **[x] Swift** (extensions on `Node` and `KeyStore`, `Aviary.restoreNode`,
  `splitBlobList`, `NetNode`, `SecureSession`, `SecureIdentity`,
  `ConsensusStep`, `Relay`, `TimeoutRequest`, `Outbound`; the event hook
  keeps closures in a file private dictionary keyed by the node handle and
  `Node.deinit` unhooks). `main.swift` exercises all of it; `precondition`
  cannot take `try` in its autoclosure, hence the `ensure` helper.
- Every binding now wraps the entire C ABI again.

## Distribution: npm bundled natives, Go module fetchable, Swift by URL, JVM jar - DONE (2026-10-11)
- **[x] npm `aviary-ledger` 0.1.0** ships `prebuilds/{darwin-arm64,linux-x64,
  linux-arm64}` (the aviary_dart libraries; `scripts/node/stage_prebuild.sh`
  copies them), an `.npmignore` so the root `.gitignore` no longer strips the
  binaries from the tarball, `repository`/`os`/`cpu` fields, a CHANGELOG and
  LICENSE. `resolveCryptolib` prefers the `libcipherbird` sibling. All 16 tests
  pass with no environment override; `npm pack` is 21 MB. `publish.yml` gained
  an `npm` job on `aviary-ledger-vX.Y.Z` tags using npm trusted publishing with
  provenance (enable the trusted publisher on npmjs.com after the first manual
  publish).
- **[x] Go**: module path is now lowercase `github.com/Crdzbird/aviary/bridge/
  bindings/go`; it requires CipherBird's Go module at the published pseudo
  version `v0.0.0-20261005192042-bffafd77df2f` instead of a local replace.
  CipherBird's module keeps its C header outside the module, so a fetched copy
  builds only with `-tags aviary_pkgconfig,cryptolib_pkgconfig` against
  installed libraries (documented); inside the repo the root `go.work`
  (binding, three example servers, sibling CipherBird) makes plain `go build`
  work. Tag `bridge/bindings/go/v0.1.0`.
- **[x] Swift**: `Package.swift` moved to the repository root (SwiftPM resolves
  git URL dependencies only from there) with target paths under
  `bridge/bindings/swift/`. Binary targets default to release URLs with
  checksums; `AVIARY_LOCAL_XCFRAMEWORKS=1` switches to the local trees for
  the demo. `scripts/apple/package_swift_release.sh swift-v0.1.0` produced
  `dist/swift/{AviaryC,CipherBird}.xcframework.zip` and wrote the checksums
  into the manifest. The zips must be attached by the repository owner to a
  GitHub release named `swift-v0.1.0` (my GitHub identity has no write access).
- **[x] JVM**: `scripts/jvm/build_jar.sh 0.1.0` builds `dist/jvm/aviary-0.1.0.jar`
  (21 MB) with every native slot under `/native/<os>-<arch>/` plus an
  `index.txt`; `NativeLoader` extracts the whole slot into one temporary
  directory so the darwin pair keeps its loader relative link. Verified by
  running the jar from `/tmp` with no path override. Publishing to a Maven
  repository is left to the owner's coordinates.
