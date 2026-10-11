# Aviary

A C++20 distributed ledger toolkit: build custom chains, run them privately
or permissionlessly, connect them to other chains, and stream, observe and
verify everything. One C ABI carries the whole engine to Dart, Flutter, Go,
npm, JVM and Swift; consensus logic lives once, in C++, and the bindings only
marshal. All cryptography comes from
[CipherBird](https://github.com/Crdzbird/CipherBird), the sibling library
checked out at `../cryptolib`.

> Status: the core is complete through BFT finality with BLS commit
> certificates, account and UTXO state machines, composable chains (engine,
> state model and proposer mixtures, foreign components written in the
> binding language), subchains with cross chain proofs, validator rotation,
> light clients with the overlap rule and bisection, chain trees, a networked
> node over an application owned transport with post quantum hybrid secure
> sessions, custody (key stores, portable and threshold accounts), assurance
> postures, sealed mempools, anonymous membership credentials, checkpointed
> snapshots, typed events and Prometheus metrics. Golden vectors round trip
> identically through C++, Dart and Go, the decoders are fuzzed, and the
> shipped engines are trimmed (Linux 7 MB, Android 6 MB). EVM, Solana and
> Bitcoin interop (addresses, transfers, signatures, RPC clients) is
> validated against real on-chain vectors. See
> [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design,
> [`docs/CODEC_SPEC.md`](docs/CODEC_SPEC.md) for the canonical codec, and
> [`docs/IMPROVEMENTS.md`](docs/IMPROVEMENTS.md) for the running backlog.

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

# libFuzzer harnesses: codec, persistence and networking decoders, C ABI inputs
cmake -S . -B build-fuzz -DAVIARY_BUILD_FUZZERS=ON -DAVIARY_SANITIZE=ON
cmake --build build-fuzz --target fuzz_codec fuzz_restore fuzz_c_abi
./build-fuzz/fuzz_c_abi -max_total_time=60
# Apple clang has no libFuzzer: add -DAVIARY_FUZZER_STANDALONE=ON and run a target over seed files
# ThreadSanitizer build of the registry concurrency test
cmake -S . -B build-tsan -DAVIARY_SANITIZE_THREAD=ON && cmake --build build-tsan --target test_registry_threads && ./build-tsan/test_registry_threads
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
