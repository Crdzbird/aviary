# Changelog

## 0.1.0

- `PeerMesh`: a deterministic in-memory mesh of `NetNode`s (`link`,
  `linkAll`, `start`, `settle`, `advance`, `run`, `step`, `deliver`) for
  tests, simulations and single process networks.
- Events and metrics on `Chain` and `NetNode`: `events()`, `eventStream`,
  `metrics()` and `metricValues()`, matching `Node`. `Chain.free` and
  `NetNode.free` close a live stream.
- First release. The ledger (accounts, signed transfers, BFT finality,
  mempool, events, metrics, state and inclusion proofs), composable chains
  (mixtures of engine, state model and proposer; foreign components written
  in Dart), subchains (anchoring, cross chain proofs), validator rotation,
  light clients with the overlap rule and bisection, chain trees, custody
  (key stores, portable accounts, threshold accounts), assurance postures,
  sealed mempools and anonymous membership credentials.
- Shortcut extensions: hex helpers on Uint8List and String, funded
  accounts, auto nonce transfers and one call blocks on Node, single
  validator chain assembly, whole threshold ceremonies, fluent chain trees.
- The smoke scripts are `package:test` suites: `dart test` runs them.
- Linux x64 and arm64 engines ship in the package and are bundled by the
  build hook (`hook/build.dart`) as a code asset; a Windows one follows the
  same path once it exists;
  `Aviary.loadedFrom` reports how the engine was found. Requires Dart 3.10.
- Custody signing: `KeyStore.signTransfer`, `Node.sendFromStore`,
  `Node.submitTransferFromStore` and `Node.submitTx`; the sealed secret is
  used inside the engine and wiped. `KeyStore` gains `publicKey`, `has`,
  `addresses` and `lock`.
- Networking: `NetNode` runs BFT consensus over a gossip mesh with the app
  owning sockets and timers (`start`, `onGossip`, `onTimeout`, `advance`,
  `setTransactions`, `replayLog`); `SecureSession` protects each link.
- Persistence: `snapshot`, `restoreNode` (constant time from the carried
  ledger state, or `replay: true` to audit the archive), `saveTo`,
  `restoreNodeFrom` and `blockAt`.
- `nextNonce` counts pending transfers; `send`, `sendTo` and `sendFromStore`
  use it, so several transfers per account can be queued before a block.
- Typed chain events (`BlockFinalized`, `BlockRejected`, `TxAdmitted`,
  `TxRemoved`, `UnknownEvent`) from `events()` and the push based
  `eventStream`, fed by a native hook instead of polling.
- Native handles are freed by a finalizer when their Dart object is
  collected; releasing twice is a no-op and `isReleased` reports the state.
  `withNode`, `withKeyStore` and `withTree` run a body and release after.
- `AviaryException.status` carries the C ABI status (`AviaryStatus`), so
  a credit after the first block is distinguishable as `genesisSealed`.
- `sealShareAsync`, `unsealShareAsync`, `sealAccountPassphraseAsync` and
  `openAccountPassphraseAsync` run the Argon2 bound operations on a worker
  isolate.
- The CipherBird engine is bundled for macOS arm64; other hosts load the
  library from AVIARY_DYLIB.
