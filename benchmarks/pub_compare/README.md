# Aviary against pub.dev

pub.dev has no other embeddable ledger engine for Dart: nothing that combines
signed transactions, a mempool, BFT finality, state commitments and proofs.
What it has are the pieces a Dart team would build one from. This package
measures Aviary against those pieces, and against a ledger assembled from
them, on the same machine and in the same process.

## What was compared

| Package | Version | Role here | Implementation |
|---|---|---|---|
| `crypto` | 3.0.7 | SHA-256 | pure Dart |
| `hashlib` | 2.4.2 | SHA-256, BLAKE2b-256 | pure Dart |
| `blake3_ffi` | 1.2.5 | BLAKE3 | native, FFI |
| `ed25519_edwards` | 0.3.2 | Ed25519 | pure Dart |
| `pinenacl` | 0.6.0 | Ed25519 (TweetNaCl) | pure Dart |
| `cryptography` | 2.9.0 | Ed25519 (`DartEd25519`) | pure Dart |
| `pqcrypto` | 0.4.3 | ML-DSA-65 | pure Dart |
| `dart_merkle_lib` | 1.0.1 | Merkle roots | pure Dart |

`HandLedger` (`lib/hand_ledger.dart`) is the ledger those packages make:
Ed25519 signatures from `ed25519_edwards`, nonce and balance checks in maps,
BLAKE2b transaction ids, and `dart_merkle_lib` roots over the transactions
and the state. It has no consensus, no second validation pass and no
certificate, so it does less work per block than Aviary.

Left out on purpose:

- `lunaris_engine` lists PBFT, proof of stake, Ed25519 and BLS, but its
  signatures hash with a placeholder (`data[i % len] ^ i`) and its PBFT only
  counts quorums. Timing it would compare against cryptography that does not
  exist.
- `bls_dart` verifies BLS signatures but cannot create them and needs a
  flutter_rust_bridge runtime.
- `blockchain_utils`, `coinlib`, `flutterchain`, `activeledger` and the chain
  SDKs are clients for existing networks, not ledgers.
- `origami_blockchain` (2021) and `merkletree` (2019) are unmaintained.

## Results

Apple M3 Max, Dart 3.13.5, Aviary 0.1.0 on CipherBird 1.1.1, measured on
the engine of commit 4f6ba2b. The engine and binding work recorded in
`docs/PERFORMANCE.md` (one signature check per transaction, cached symbol
lookups) has made the Aviary rows faster since; the pub.dev rows are
unchanged. Each figure is
the best of five interleaved rounds per contender, so a burst of load
elsewhere costs one round, not the result. Time per operation; lower is
better. AOT is what a Flutter release build runs.

### Hashing

| Work | Library | `dart run` (JIT) | `dart build cli` (AOT) |
|---|---|---|---|
| Hash 32 B | aviary_dart, BLAKE3 (FFI) | 4.14 us | 4.31 us |
| Hash 32 B | blake3_ffi, BLAKE3 (FFI) | 0.40 us | 0.32 us |
| Hash 32 B | hashlib, BLAKE2b-256 | 0.71 us | 0.95 us |
| Hash 32 B | hashlib, SHA-256 | 1.28 us | 1.43 us |
| Hash 32 B | crypto, SHA-256 | 0.93 us | 1.17 us |
| Hash 1 KiB | aviary_dart, BLAKE3 (FFI) | 4.86 us | 6.05 us |
| Hash 1 KiB | blake3_ffi, BLAKE3 (FFI) | 1.68 us | 1.88 us |
| Hash 1 KiB | hashlib, BLAKE2b-256 | 5.68 us | 9.13 us |
| Hash 1 KiB | hashlib, SHA-256 | 12.4 us | 15.0 us |
| Hash 1 KiB | crypto, SHA-256 | 12.5 us | 13.4 us |
| Hash 64 KiB | aviary_dart, BLAKE3 (FFI) | 72.6 us | 64.4 us |
| Hash 64 KiB | blake3_ffi, BLAKE3 (FFI) | 66.5 us | 48.7 us |
| Hash 64 KiB | hashlib, BLAKE2b-256 | 329.9 us | 529.2 us |
| Hash 64 KiB | hashlib, SHA-256 | 905.1 us | 1.12 ms |
| Hash 64 KiB | crypto, SHA-256 | 926.6 us | 910.6 us |

### Ed25519

| Work | Library | `dart run` (JIT) | `dart build cli` (AOT) |
|---|---|---|---|
| Ed25519 sign | ed25519_edwards | 328.5 us | 807.8 us |
| Ed25519 verify | ed25519_edwards | 591.3 us | 1.74 ms |
| Ed25519 sign | pinenacl | 1.63 ms | 1.16 ms |
| Ed25519 verify | pinenacl | 2.77 ms | 2.10 ms |
| Ed25519 sign | cryptography (pure Dart) | 2.69 ms | 2.13 ms |
| Ed25519 verify | cryptography (pure Dart) | 2.93 ms | 2.26 ms |
| Ed25519 transfer: sign, verify, admit | aviary_dart (engine) | 92.3 us | 78.7 us |

### Post quantum signatures

| Work | Library | `dart run` (JIT) | `dart build cli` (AOT) |
|---|---|---|---|
| ML-DSA-65 sign | pqcrypto (pure Dart) | 7.92 ms | 6.21 ms |
| ML-DSA-65 verify | pqcrypto (pure Dart) | 2.76 ms | 2.31 ms |
| Hybrid transfer: sign and verify both | ed25519_edwards + pqcrypto | 11.55 ms | 10.47 ms |
| Hybrid transfer: sign, verify both, admit | aviary_dart (engine) | 793.2 us | 795.7 us |

### A block of 1,000 transfers

| Work | Library | `dart run` (JIT) | `dart build cli` (AOT) |
|---|---|---|---|
| Sign, verify and admit 1,000 transfers | hand built: ed25519_edwards + hashlib + dart_merkle_lib | 953.6 us | 2.64 ms |
| Produce the block of 1,000 | hand built: ed25519_edwards + hashlib + dart_merkle_lib | 6.60 us | 5.73 us |
| Admit and finalize, end to end | hand built: ed25519_edwards + hashlib + dart_merkle_lib | 962.2 us | 2.65 ms |
| Sign, verify and admit 1,000 transfers | aviary_dart (engine, with BFT certificate) | 83.7 us | 87.2 us |
| Produce the block of 1,000 | aviary_dart (engine, with BFT certificate) | 151.3 us | 173.6 us |
| Admit and finalize, end to end | aviary_dart (engine, with BFT certificate) | 235.0 us | 260.8 us |
| Follower: verify and apply received blocks | hand built: ed25519_edwards + hashlib + dart_merkle_lib | 807.0 us | 2.36 ms |
| Follower: verify and apply received blocks | aviary_dart (engine, with BFT certificate) | 101.9 us | 79.4 us |

## Reading the results

- **Ledger work.** Admitting and finalizing a transfer costs 0.26 ms in
  Aviary and 2.65 ms in the hand built ledger under AOT, about 10 times
  less, while Aviary also validates each block a second time and certifies
  it with BLS. A follower that verifies received blocks runs about 30 times
  faster under AOT. The hand built ledger only wins at producing a block,
  because it skips that validation and the certificate.
- **Signatures.** Aviary signs, verifies and admits an Ed25519 transfer
  faster than the fastest pure Dart package only signs. `ed25519_edwards`
  stays the fastest pure Dart Ed25519 under AOT but slows about 2.5 times
  (0.81 ms to sign, 1.74 ms to verify), while `pinenacl` and `cryptography`
  get about a quarter faster.
- **Post quantum.** A hybrid Ed25519 + ML-DSA-65 transfer is 13 to 15 times
  cheaper in Aviary than signing and verifying the same pair with
  `ed25519_edwards` and `pqcrypto`.
- **Hashing is where Aviary loses.** `aviary_dart`'s `hash` takes about
  4 us for 32 bytes against 0.3 to 1.4 us for every other package. The cost
  is CipherBird returning each BLAKE3 digest in locked memory: 2.3 of the
  2.5 us the engine spends per call is the lock and unlock. With plain
  output the engine call takes 0.22 us (measured with a patched CipherBird);
  the Dart binding adds its own copies on top, so the Dart figure would land
  near `blake3_ffi` instead of ten times above it. Hashes inside the engine
  (block, transaction and state hashing) pay the same cost, so the fix also
  speeds up the ledger rows above. At 64 KiB the two BLAKE3 packages are
  within a third of each other and 5 to 23 times ahead of the pure Dart
  hashes.
- **Merkle trees.** `dart_merkle_lib` duplicates the last node of an odd
  level, the construction behind CVE-2012-2459 that lets two different
  transaction lists share a root. Aviary carries a lone node up instead.

## Run it

```bash
dart pub get
dart run bin/compare.dart
```

For the AOT column:

```bash
dart build cli -t bin/compare.dart -o build/aot
./build/aot/bundle/bin/compare
```

Run from this directory so `aviary_dart` finds its engine through the
package config, or point `AVIARY_DYLIB` at a `libaviary_c` build. A run
takes about a minute per mode.
