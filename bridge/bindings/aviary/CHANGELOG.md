# Changelog

## 1.1.1

The bundled AviaryC frameworks and Android libraries are rebuilt from
Aviary 1.1.1 on CipherBird 1.6.0. Requires `aviary_dart` 1.1.1.

- Snapshot restore verifies every archived commit certificate.
  `restoreChain`, `restoreChainTree` and `chainSnapshotError` used to check
  only that an archived certificate below the tip named its block, so a
  snapshot whose certificate signature or signer bitmap had been altered
  restored and `archivedCertificate` then returned the forged certificate.
  Each certificate is now verified (signer list, quorum power, aggregate BLS
  signature) under the validator set in force at its height, following
  validator rotations. When that set is no longer in the snapshot (it was
  announced in a block pruned from the archive and has since rotated out),
  the certificate must equal the last commit embedded in the next archived
  header. A snapshot that fails either check is refused.
- A snapshot of a chain other than BFT that carries an archive is refused.

## 1.1.0

- Requires `cipherbird` 1.6.0. CipherBird renamed its C ABI to
  `cipherbird_*`, and the bundled AviaryC frameworks and Android library
  now resolve those names against the cipherbird package's engine, which
  `cipherbird` 1.3.0 does not export.
- Through CipherBird 1.6.0, passphrase sealing (`sealShare`,
  `sealAccountPassphrase`) refuses an Argon2id memory cost that is not a
  multiple of 1024 bytes; zero still selects the sensitive preset.
- Subchains version 2 and persistence from `aviary_dart`, re-exported:
  dynamic child registration signed by registrars, a Merkle registry with
  one provable `ChildRecord` per child, closing and registrar rotation,
  version 2 cross chain proofs, chain snapshots with an opt in block
  archive, and chain tree snapshots. The bundled native libraries are
  rebuilt with these functions.
- Web: through `aviary_dart` 1.1.0 the package compiles for the web, where
  `ensureInitialized` loads the WebAssembly engine and the stateless calls
  (hashing, header hashes, proof and certificate checks) work in a browser.
  Await `ensureInitialized` before `instance` on the web.
- The engine reports the CipherBird revision it was built from in
  `cryptoBackend()`.

## 1.0.0

First stable release: the Aviary ledger for Flutter on Android, iOS and
macOS. The public API follows semantic versioning
from this release on.

- The whole `aviary_dart` API: the ledger with BFT finality, composable
  chains with application records, subchains, validator rotation, light
  clients, chain trees, networking with `NetNode`, `SecureSession` and
  `PeerMesh`, custody, assurance postures, sealed mempools, anonymous
  membership credentials, typed event streams and metrics, and record
  queries on networked nodes.
- Shares the native engine with the `cipherbird` package (1.3.0 or later)
  and re-exports its Dart API, so the engine exists once in the app.
- `AviaryShared` entry point (`ensureInitialized`, `instance`) and the shortcut
  extensions of `aviary_dart`.
- Requires Android API 24, iOS 15 and macOS 12, matching the CipherBird
  engine.
- A four tab example app: ledger, events, records and network.
- Ships an Apple privacy manifest: no tracking, no collected data, and the
  file timestamp and system boot time reasons declared.
