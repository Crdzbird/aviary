# Changelog

## 0.1.0

- First release of aviary: the Aviary ledger, composable chains,
  subchains, validator rotation, light clients, chain trees, custody and
  anonymous membership credentials for Flutter over dart:ffi.
- Shares the native engine with the cipherbird package and re-exports its Dart API.
- `AviaryShared` entry point (`ensureInitialized`, `instance`) and the
  shortcut extensions of aviary_dart.
- Requires iOS 15 and macOS 12, matching the CipherBird engine inside.
