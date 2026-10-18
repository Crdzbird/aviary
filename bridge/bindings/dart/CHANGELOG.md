# Changelog

## 0.1.0

First release of the Dart API for the Aviary ledger, built on the
CipherBird 1.1.1 crypto engine.

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
  `proveRecord` and `recordFromProof` cover the whole lifecycle, and
  `Chain.addTx` queues any signed transaction on any assembled chain.
- Subchains with anchoring and cross chain proofs, validator rotation,
  light clients with the overlap rule and bisection, and chain trees that
  build and produce a whole family of chains.

**Networking**

- `NetNode` runs BFT consensus over a gossip mesh while the app owns
  sockets and timers; `SecureSession` gives each link a mutually
  authenticated, post quantum hybrid channel.
- `PeerMesh`: a deterministic in-memory mesh of `NetNode`s for tests,
  simulations and single process networks.

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

- The engine ships for macOS arm64 and Linux x64 and arm64; the build hook
  bundles the Linux library as a code asset, and `Aviary.loadedFrom`
  reports which library was loaded. `AVIARY_DYLIB` points at any other
  build.
- Native handles are released by a finalizer; releasing twice is a no-op.
  `withNode`, `withKeyStore` and `withTree` release at the end of a scope.
- `AviaryException.status` carries the C ABI status (`AviaryStatus`).
- Shortcut extensions: hex helpers, funded accounts, one call blocks,
  single validator chains, whole threshold ceremonies, fluent chain trees.
- API docs are grouped into dartdoc categories. Requires Dart 3.10.
