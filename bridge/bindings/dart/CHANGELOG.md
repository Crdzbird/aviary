# Changelog

## Unreleased

The bundled macOS engine is rebuilt with the functions below. The Linux
engines are rebuilt by the Native Linux workflow once this source is pushed;
until then a Linux build needs `AVIARY_DYLIB` pointing at an engine built
from the same source.

**Packaging**

- The build hook now bundles the macOS engine as a code asset too, so a
  `dart build cli` bundle carries `libaviary_c` in `bundle/lib` and runs
  with no package config and no `AVIARY_DYLIB`. On macOS the hook leaves the
  engine to the `aviary` and `aviary_lite` plugins when the app depends on
  one of them, so an app never carries two.

**Subchains version 2**

- A parent with a dynamic policy (`encodeDynamicSubchainPolicy`, registrars
  as `RegistrarEntry`) registers children in any block: `opRegister`,
  `opClose`, `opSetRegistrars`, `opDigest`, `signOp`, `addOpApproval` and
  `Chain.proposeSubchain`.
- A Merkle registry with one `ChildRecord` per child: `Chain.childRecord`,
  `Chain.childProof`, `Chain.registryCounts`, `verifyChildProof`,
  `pendingAnchor`, `Chain.subchainHopV2`, `encodeSubchainProofV2`,
  `verifySubchainProofV2` and `encodedHeaderHash`.

**Persistence**

- `Chain.snapshot`, `restoreChain` and `chainSnapshotError`; an opt in block
  archive (`Chain.enableArchive`, `archiveRange`, `archivedBlock`,
  `archivedCertificate`, `pruneArchive`); `Chain.tip`.
- `ChainTree.snapshot` and `restoreChainTree`.

## 1.0.0

First stable release of the Dart API for the Aviary ledger, built on the
CipherBird 1.3.0 crypto engine. The public API follows semantic versioning
from this release on.

**Ledger**

- `Node`: accounts (Ed25519 and hybrid Ed25519 + ML-DSA-65), signed
  transfers, a mempool, BFT finality with BLS aggregated certificates,
  state and transaction inclusion proofs.
- `nextNonce` counts transfers still pending, and `send`, `sendTo` and
  `sendFromStore` use it, so several transfers per account can be queued
  before a block.
- Persistence: `snapshot`, `restoreNode` (resumes from the carried ledger
  state, or `replay: true` to audit every block), `saveTo`,
  `restoreNodeFrom` and `blockAt`.

**Chains**

- Composable chains: blueprints that mix a consensus engine (`bft`, `pos`,
  `vrf-pos`, `pow`), a state model (`account`, `utxo`, `record`) and a
  proposer policy, committed into the genesis hash. Foreign engines and
  proposers written in Dart.
- Application records: `assembleRecordChain` builds a chain that stores
  owned, versioned objects instead of balances. `Chain.write`, `record`,
  `proveRecord` and `recordFromProof` cover the whole lifecycle;
  `writeFromStore` signs inside the engine from a `KeyStore` and
  `signRecordTxHybrid` signs with a hybrid post quantum account.
  `Chain.addTx` queues any signed transaction on any assembled chain, and
  the queue reports `TxAdmitted` and `TxRemoved` events.
- Subchains with anchoring and cross chain proofs, validator rotation,
  light clients with the overlap rule and bisection, and chain trees that
  build and produce a whole family of chains.

**Networking**

- `NetNode` runs BFT consensus over a gossip mesh while the app owns
  sockets and timers; `SecureSession` gives each link a mutually
  authenticated, post quantum hybrid channel.
- `PeerMesh`: a deterministic in-memory mesh of `NetNode`s for tests,
  simulations and single process networks.
- `AddressBook` and self signed peer records with verified peer exchange,
  `NoiseSession` (Noise XX with a post quantum identity proof), and the
  anonymous membership admission handshake (`admissionChallenge`,
  `admissionAnswer`, `admissionGranted`).
- Record model networks: `NetNode.record`, `recordNonce` and `proveRecord`
  answer on every node, observers included. `setTransactions` lists are
  cleared when the height commits, so the next height's list is set before
  `advance` and reaches its round 0 proposal.

**Custody, assurance and credentials**

- `KeyStore` seals secrets at rest and signs inside the engine
  (`signTransfer`, `sendFromStore`, `submitTransferFromStore`), with
  `lock`, `unlock`, `publicKey`, `has` and `addresses`.
- Portable accounts sealed to a passphrase, a recipient or with a
  signature; threshold accounts with ceremonies and sealed shares. Argon2
  bound operations have async variants that run on a worker isolate.
- Assurance postures, sealed mempools and anonymous membership credentials
  with an optional pseudonymous mode.

**Observability**

- Typed events (`BlockFinalized`, `BlockRejected`, `TxAdmitted`,
  `TxRemoved`, `UnknownEvent`) from `events()` and a push based
  `eventStream`, plus Prometheus metrics, on `Node`, `Chain` and `NetNode`.

**Packaging and ergonomics**

- Native symbols are resolved once per library, and each call's inputs
  share one native block that is wiped before it is freed, so secret keys
  do not linger in freed memory.

- The engine ships as one self contained library per platform (macOS arm64
  2.8 MB, Linux x64 and arm64); the build hook
  bundles the Linux library as a code asset, and `Aviary.loadedFrom`
  reports which library was loaded. `AVIARY_DYLIB` points at any other
  build.
- Native handles are released by a finalizer; releasing twice is a no-op.
  `withNode`, `withKeyStore` and `withTree` release at the end of a scope.
- `AviaryException.status` carries the C ABI status (`AviaryStatus`).
- Shortcut extensions: hex helpers, funded accounts, one call blocks,
  single validator chains, whole threshold ceremonies, fluent chain trees.
- API docs are grouped into dartdoc categories. Requires Dart 3.10.
