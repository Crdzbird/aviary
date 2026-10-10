# Changelog

## 0.1.0

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
- Networking: `NetNode` runs BFT consensus over a gossip mesh with the app
  owning sockets and timers (`start`, `onGossip`, `onTimeout`, `advance`,
  `setTransactions`, `replayLog`); `SecureSession` protects each link.
- Persistence: `snapshot`, `restoreNode`, `saveTo`, `restoreNodeFrom` and
  `blockAt`. A restored node replays its blocks through consensus and keeps
  producing.
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
