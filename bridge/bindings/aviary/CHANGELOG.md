# Changelog

## 0.1.0

First release: the Aviary ledger for Flutter on Android, iOS and macOS.

- The whole `aviary_dart` API: the ledger with BFT finality, composable
  chains with application records, subchains, validator rotation, light
  clients, chain trees, networking with `NetNode`, `SecureSession` and
  `PeerMesh`, custody, assurance postures, sealed mempools, anonymous
  membership credentials, typed event streams and metrics, and record
  queries on networked nodes.
- Shares the native engine with the `cipherbird` package (1.1.1 or later)
  and re-exports its Dart API, so the engine exists once in the app.
- `AviaryShared` entry point (`ensureInitialized`, `instance`) and the shortcut
  extensions of `aviary_dart`.
- Requires Android API 24, iOS 15 and macOS 12, matching the CipherBird
  engine.
- A four tab example app: ledger, events, records and network.
- Ships an Apple privacy manifest: no tracking, no collected data, and the
  file timestamp and system boot time reasons declared.
