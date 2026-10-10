# Changelog

## 0.1.0

- First release. The ledger (accounts, signed transfers, BFT finality,
  mempool, events, metrics, state and inclusion proofs), composable chains
  (mixtures of engine, state model and proposer; foreign components written
  in Dart), subchains (anchoring, cross chain proofs), validator rotation,
  light clients with the overlap rule and bisection, chain trees, custody
  (key stores, portable accounts, threshold accounts), assurance postures,
  sealed mempools and anonymous membership credentials.
- The CipherBird engine is bundled for macOS arm64; other hosts load the
  library from BLOCKCHAINER_DYLIB.
