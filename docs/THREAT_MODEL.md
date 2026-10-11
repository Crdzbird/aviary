# Aviary - Threat Model

> The adversary model, per-component threats with their mitigations and status,
> the security arguments for the original constructions, and the residual-risk
> register. Composes with cryptolib's own threat model
> ([`../cryptolib/docs/THREAT_MODEL.md`](../../cryptolib/docs/THREAT_MODEL.md)) - 
> all cryptographic primitive security is delegated there; this document covers
> how aviary *uses* them.
>
> Status: **M7 living document.** Reflects the implemented system. Items marked
> *(deferred)* are tracked in [`IMPROVEMENTS.md`](IMPROVEMENTS.md).

---

## 1. Assets

1. **Ledger integrity** - no two honest nodes finalize conflicting state; balances/
   supply are conserved; history is append-only and tamper-evident.
2. **Funds** - account balances / UTXOs; bridge escrow.
3. **Validator & wallet keys** - Ed25519 / BLS / secp256k1 secrets.
4. **Liveness** - the chain keeps producing and finalizing blocks.
5. **Channel confidentiality** - P2P traffic on a secure session.

## 2. Adversary model

Assumed capabilities (always):
- **Network**: reorder, drop, delay, duplicate, and inject messages; run Sybil
  and eclipse attempts; flood/DoS.
- **Byzantine validators**: up to `f` of `n` may deviate arbitrarily (equivocate,
  withhold, propose invalid blocks). BFT safety holds for `f < n/3` by voting
  power.
- **Malicious input**: craft arbitrary bytes for any decoder, malformed txs/
  blocks/proofs/packets, adversarial RPC responses.
- **Harvest-now-decrypt-later**: record traffic / signatures now to break later
  with a quantum computer - motivates the PQ-hybrid posture.

Out of scope: a malicious cryptolib build; compromise of the host OS / RNG; a
quorum (>=1/3 power) of colluding validators; physical key extraction.

## 3. The two invariants we never break

- **Safety over liveness.** If safety and liveness conflict, the chain **halts**
  rather than forks (BFT locking + `f < n/3`). A halted chain recovers; a forked
  one loses funds.
- **Determinism.** Every honest node derives byte-identical state from identical
  input, or the chain splits. Enforced by canonical serialization, no ambient
  nondeterminism in validation, and checked/`u128` value math. See
  [`ARCHITECTURE.md`](ARCHITECTURE.md) section 2.

## 4. Threats by component

### 4.1 Consensus (BFT)
| Threat | Mitigation | Status |
|---|---|---|
| Two conflicting blocks finalized at one height | Quorum intersection: a commit needs >2/3 power; two quorums share >1/3 -> >=1 honest signer would have to double-sign. Asserted in tests. | [x] |
| Forged commit certificate | BLS aggregate-verify over per-validator index-bound messages; strictly-ascending unique signers; power >= quorum. | [x] |
| Rogue-key attack on the aggregate | Each vote binds the validator index -> distinct messages -> BLS *core* AggregateVerify (no proof-of-possession needed). | [x] |
| Equivocation (double-sign) detection/slashing | Not detected or punished (prevention only). | *(deferred)* |
| Long-range / nothing-at-stake | N/A for BFT instant finality; relevant only to the deferred PoS engine. | *(deferred w/ PoS)* |
| Liveness loss (>=1/3 offline) | Chain halts, does not fork (by design). | [x] |

### 4.1b Validator-set changes (rotation)
| Threat | Mitigation | Status |
|---|---|---|
| A proposer swaps the validator set unilaterally | The announcing block needs >2/3 of the CURRENT set; each validator only votes for a change it scheduled itself (governance at vote time). | [x] |
| A certified rotation is ignored by some node (fork) | Finalization follows the certificate regardless of local schedule (`finalizable` split from `valid`); the node remaps its index or stops voting if it left. | [x] |
| Old validators keep signing after being rotated out | The next header commits the NEW set's hash; old-index votes no longer verify; a block "certified" by the old set is refused. | [x] |
| Malformed announced set (empty, bad key, zero power) | Checked well-formed at validation, before it can take effect. | [x] |
| Light client resumed with the wrong set across a rotation | Needs both the set in force next and the set that certified its tip; otherwise the next header fails. | [x] |
| Long-range attack on a light client offline across many rotations | Overlap (skipping) rule: a far header is trusted only if its own set certifies it AND signers holding > 1/3 of the client's trusted set signed it; bisection otherwise. Sound only within the trusting period - `max_skip` is the operator's bound (no consensus clock), documented in `light_client.hpp`. | [x] (trusting period: *operator-bound*) |

### 4.2 Serialization / codec (the prime untrusted-input surface)
| Threat | Mitigation | Status |
|---|---|---|
| Hash malleability (two encodings -> one object) | One canonical encoding; decoder **rejects** non-canonical forms (trailing bytes, non-minimal biguint, out-of-range u64, bad bool) instead of normalizing. | [x] |
| Merkle duplicate-node malleability (CVE-2012-2459) | Lone node carried up unchanged, never duplicated. | [x] |
| Allocation-DoS via huge declared lengths | Length/count bounded by `kMax*` **and** checked against bytes remaining before allocating. | [x] |
| Memory-safety bug in a decoder | Differential/property + fuzz tests; **20k-iteration totality fuzz + ASan/UBSan clean** across all core and interop decoders. | [x] |
| Integer overflow on value math (balance/supply/escrow) | `u128` with explicit overflow checks (account credit, UTXO sums, bridge mint **and escrow**). | [x] |

### 4.3 Networking / P2P
| Threat | Mitigation | Status |
|---|---|---|
| Eavesdropping / tampering on the wire | Secure session: hybrid-KEM (X25519+ML-KEM-768) key agreement + SecretStream AEAD; tamper/order/truncation detected. | [x] |
| MITM on first contact (no peer-identity auth) | **Not** mitigated - channel is confidential + PQ-hybrid but not identity-authenticated. | *(deferred)* |
| Replayed / out-of-order frames | SecretStream enforces stream order; replays fail authentication. | [x] |
| Malformed wire frames | Canonical wire codec; unknown message types and trailing bytes rejected; fuzzed. | [x] |
| Mempool spam / DoS | Per-tx signature + chain-id + size + dedup checks; capacity cap. Fee-priority eviction, per-peer rate limiting | partial; *(deferred)* |
| Sybil / eclipse | Gossip overlay, peer discovery/scoring, diverse peer selection. | *(deferred - M3 cont.)* |

### 4.4 Verification / light client
| Threat | Mitigation | Status |
|---|---|---|
| Light client accepts an invalid header | Verifies each header's commit certificate against the validator set + parent linkage; trusts roots only after >2/3 attests. | [x] |
| Forged inclusion proof | Domain-separated Merkle leaf hashing; proof recomputes the trusted root; tampered proof / wrong root rejected. | [x] |
| Proving non-membership | Only presence proofs supported; absence proofs not yet verifiable. | *(deferred)* |
| Skipping to a header the trusted set never vouched for | `accept_skipping` requires signers holding > trust_level (1/3) of the TRUSTED set; a wrong or forged set fails the header's committed hash; a mis-certified header fails its own >2/3 rule. | [x] |

### 4.4b Subchains (anchoring - docs/design/SUBCHAINS.md)
| Threat | Mitigation | Status |
|---|---|---|
| Parent accepts a child header its validators never finalized | Each anchored header runs the light-client rule (height +1, parent hash, embedded last commit, quorum certificate) under the child's genesis-registered validator set; segments must be consecutive. | [x] |
| Proposer slips an invalid anchor into a certified block | `Ledger::validate_block` dry run before every prevote; anchors validated before any tx work; a bad anchor invalidates the whole block, all-or-nothing. | [x] |
| Child equivocation (two certified headers at one height) | Registry only advances by strictly increasing height; the second header can never be anchored, the parent's view cannot flip. Evidence wire type reserved, not emitted. | [x] (evidence *deferred*) |
| Child re-parented / parent swapped | `ParentBinding` committed in the child's genesis hash; registration by the child's genesis header (no hash cycle). | [x] |
| Forged cross-chain proof | Every hop recomputes `subchain_root` from a canonical (ascending, unique) list; descends only through the committed child tip; final state proof verified under that tip's root. | [x] |
| Child validator supermajority anchors a false state root | **Undetectable by the parent** (it never executes children) - the same limit as any light client. Documented, not mitigated. | *(accepted)* |
| Child rotates validators | Followed: the child's rotation header is anchored like any other, the registry tracks the set in force next + the set that certified the tip, and `subchain_root` commits both hashes (a snapshot cannot smuggle a set). | [x] |

### 4.5 Interop & bridge (highest-loss-history component)
| Threat | Mitigation | Status |
|---|---|---|
| Unauthorized mint / release | Mint/release only after a **>2/3 BLS attestation** over the packet commitment is verified. | [x] (federated trust) |
| Replay of a bridge packet | Per-`(source_chain, nonce)` processed-set; replay rejected; recorded only on success. | [x] |
| Cross-chain confusion | `dest_chain` checked against the endpoint; commitment binds source/dest/nonce/payload. | [x] |
| Escrow over-release / overflow | Release checks sufficient escrow; lock checks `u128` overflow. | [x] |
| Attestor-federation compromise (>=1/3) | Trust assumption of the federated model. A **trustless** light-client+MPT bridge removes it. | *(deferred upgrade)* |
| Adversarial RPC response (e.g., deep-nested JSON) | JSON parser **depth-bounded** (no stack-overflow DoS); hex/quantity parsers bounds-checked. | [x] (fixed in M7) |
| EVM/BTC signature correctness | secp256k1 RFC6979 + low-S (cryptolib); EVM EIP-155 and BTC BIP143 signing verified **byte-exact vs spec vectors**. | [x] |

### 4.6 Keys & crypto
All primitive security (constant-time, side channels, KAT correctness) is
**delegated to cryptolib**. aviary's responsibilities: domain-separated
hashing (distinct tags per structure), correct primitive selection per role, and
the PQ-hybrid posture for long-lived value (hybrid Ed25519+ML-DSA signatures and
X25519+ML-KEM-768 KEM available). Wallet custody uses cryptolib's Vault/Keyring.

## 5. M7 hardening actions (this milestone)

1. **Fixed: JSON parser recursion-depth DoS.** Adversarial deeply-nested RPC
   responses could overflow the stack. Now depth-bounded (limit 200), with a
   regression test.
2. **Fixed: bridge escrow overflow.** `lock()` now checks `u128` overflow,
   matching mint/release.
3. **Broadened fuzzing** to all interop decoders (RLP, JSON, Base58/Base64,
   compact-u16, bridge packets) - 140k random inputs per run, ASan/UBSan clean.
4. **Full-suite ASan + UBSan** confirmed clean (26 suites).

## 6. Assurance posture

- **26 test suites**, including end-to-end + adversarial scenarios per component.
- **Determinism**: golden vectors replayed identically through C++, Dart, and Go.
- **Real-world validation**: EIP-55, EIP-155, ERC-20 selectors, BIP143, BIP173,
  RFC 4648, the secp256k1 generator point, and Keccak/RIPEMD KATs.
- **Fuzzing**: deterministic totality + round-trip fuzz over every decoder; an
  optional libFuzzer harness (`fuzz/`).
- **Sanitizers**: ASan + UBSan clean across the whole suite
  (`-DAVIARY_SANITIZE=ON`).
- **TSan**: not yet run - no multi-threaded code paths are exercised (metrics use
  atomics). Add when the async networking/event loop lands. *(deferred)*

## 7. Residual risks (consciously accepted, tracked in IMPROVEMENTS)

- BFT round liveness machinery and **equivocation slashing** are not
  implemented. Validator-set changes exist as coordinated rotation (section 4.1b) and
  light clients skip with the overlap rule (section 4.4); staking/bonding economics
  and an enforced trusting period (there is no consensus clock) do not.
- The bridge is **federated** (trusts a >2/3 attestor set), not trustless.
- P2P lacks **peer-identity authentication**, discovery, and a gossip overlay
  with anti-eclipse/DoS controls.
- No **persistence/storage** layer yet (in-memory state).
- **Subchains** trust each child's registered validator set for the child's
  state (section 4.4b); only BFT children can be anchored in v1.
- No external **security audit** or FIPS validation of the original
  constructions; the primitives come from cryptolib's vetted upstreams.

> Standard disclaimer: provided **as is**, no warranty. Consensus and bridge code
> is high-stakes; an independent audit is required before any deployment holding
> real value.
