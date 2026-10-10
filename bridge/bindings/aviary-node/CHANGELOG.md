# Changelog

## 0.1.0

- First release of aviary-ledger. The Aviary ledger (accounts, signed
  transfers, BFT finality, mempool, events, metrics, state and inclusion
  proofs), composable chains and foreign components, subchains, validator
  rotation, light clients, chain trees, custody (key stores, portable and
  threshold accounts, key store backed signing), assurance postures, sealed
  mempools, anonymous membership credentials, node snapshots, the push event
  hook, and networking (NetNode over an application owned transport,
  SecureSession links).
- Native libraries bundled for macOS arm64 and Linux x64 and arm64; the
  engine loads on first use or through preload().
