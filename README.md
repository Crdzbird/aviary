# Aviary

A **C++** distributed-ledger toolkit — build custom chains, run them privately or
permissionlessly, connect to other chains, and stream/observe/verify everything —
exposed through a **C ABI** with Dart, Go, npm, Swift and JVM bindings. All
cryptography comes from [CipherBird](https://github.com/Crdzbird/CipherBird)
(formerly cryptolib, still at `../cryptolib` locally); consensus logic lives once,
in C++, and the bindings only marshal.

> Status: **M0–M2 complete + M3 networking core.** A canonical codec, Merkle
> tree, block/tx model with Ed25519 signing, pluggable account **and** UTXO state
> machines, a single-node validate/apply pipeline, a Tendermint-style BFT engine
> with BLS-aggregated commit certificates and deterministic instant finality, and
> a networking core (mempool, wire protocol, a PQ-hybrid forward-secret secure
> session, and two-node gossip + sync), an observability core (typed event bus,
> Prometheus metrics, structured logging), and a verification core (light client,
> state + transaction inclusion proofs, C-ABI verification) — all in C++, with
> golden vectors (including state-proof verification) round-tripping identically
> through C++, Dart, and Go, and sanitizer-clean fuzzed decoders. Plus EVM interop,
> read and write (Keccak-256/secp256k1 via CipherBird, RLP, EIP-155 transaction
> signing + ecrecover, ABI encoding, and a JSON-RPC `eth_*` client over a
> pluggable transport), Solana transfers (Ed25519, Base58, message format + RPC
> client), and Bitcoin (Base58Check, Bech32, P2PKH/P2WPKH addresses + BIP143
> SegWit transaction signing) — all validated against real on-chain vectors, plus a
> federated cross-chain bridge (BLS-attested lock-mint / burn-release). See
> [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full design and roadmap,
> [`docs/CODEC_SPEC.md`](docs/CODEC_SPEC.md) for the canonical codec, and
> [`docs/IMPROVEMENTS.md`](docs/IMPROVEMENTS.md) for the deferred-work backlog.

## Layout

```
include/aviary/   public C++ headers
src/                    C++ core (crypto seam over the CipherBird C ABI)
bridge/aviary_c.* the C ABI every binding marshals over
bridge/bindings/dart/   aviary_dart: dart:ffi binding (pub package)
bridge/bindings/aviary_lite/  Flutter plugin, self-contained engine
bridge/bindings/aviary/       Flutter plugin over the cipherbird package
bridge/bindings/go/     cgo binding + demo
tests/                  C++ zero-dependency test runner
vectors/                language-neutral golden vectors (replayed by all langs)
docs/                   ARCHITECTURE.md, CODEC_SPEC.md
```

## Build & test

Prerequisite: build CipherBird's C ABI once (produces `libcryptolib_c`):

```bash
cd ../cryptolib && bash scripts/build_selfcontained.sh   # or: make lib
```

Then verify the whole stack (C++ + Dart + Go) in one shot:

```bash
bash scripts/build_and_test.sh
```

Or step by step:

```bash
# C++ core + C ABI + tests
cmake -S . -B build -G "Unix Makefiles"     # -DCRYPTOLIB_ROOT=<path> if not a sibling
cmake --build build
ctest --test-dir build --output-on-failure

# Dart binding
( cd bridge/bindings/dart && dart pub get && dart run bin/demo.dart )

# Go binding
( cd bridge/bindings/go && CGO_ENABLED=1 go run ./cmd/demo )
```

All three print `ALL PASS` and agree on the golden vectors in `vectors/` (BLAKE3,
canonical txid, canonical header hash).

### Hardening builds

```bash
# AddressSanitizer + UBSan over the suite (runs the fuzzed codec under sanitizers)
cmake -S . -B build-asan -DAVIARY_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure

# libFuzzer codec harness (needs a libFuzzer-capable clang)
cmake -S . -B build-fuzz -DAVIARY_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_codec
./build-fuzz/fuzz_codec -max_total_time=60
```

## Dart and Flutter packages

Three packages are published to pub.dev from this repo:

| Package | Use it when |
|---|---|
| `aviary_dart` | Pure Dart (CLI, server, tests). Bundles the macOS arm64 dylibs. |
| `aviary_lite` | Flutter app that only needs the ledger. One self-contained native binary, no `cipherbird` dependency. |
| `aviary` | Flutter app that also uses CipherBird directly. Depends on and re-exports the `cipherbird` package so the engine exists once in the app. |

`aviary_lite` and `aviary` expose the same API (everything the C ABI
offers) through `aviary_dart`. Native builds and publishing notes are in
[`docs/MOBILE.md`](docs/MOBILE.md).

## Example app: Flutter dashboard

[`examples/aviary_demo`](examples/aviary_demo) is a desktop Flutter app
that drives the whole stack through the C ABI: create a chain, send transactions,
produce blocks (BFT), watch a live event stream, read Prometheus metrics, verify a
light-client proof, and derive Ethereum/Bitcoin/Solana addresses. Build the dylib,
then `cd examples/aviary_demo && flutter run -d macos`.

A console version: `cd bridge/bindings/dart && dart run bin/node_demo.dart`.

## License

MIT (matching CipherBird).
