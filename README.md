# Blockchainer

A **C++** distributed-ledger toolkit — build custom chains, run them privately or
permissionlessly, connect to other chains, and stream/observe/verify everything —
exposed through a **C ABI** with **Dart** and **Go** bindings. All cryptography
comes from [cryptolib](../cryptolib); consensus logic lives once, in C++, and the
bindings only marshal.

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
> read and write (Keccak-256/secp256k1 via cryptolib, RLP, EIP-155 transaction
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
include/blockchainer/   public C++ headers
src/                    C++ core (crypto seam → cryptolib_c, …)
bridge/blockchainer_c.* the C ABI every binding marshals over
bridge/bindings/dart/   dart:ffi binding + demo
bridge/bindings/go/     cgo binding + demo
tests/                  C++ zero-dependency test runner
vectors/                language-neutral golden vectors (replayed by all langs)
docs/                   ARCHITECTURE.md, CODEC_SPEC.md
```

## Build & test

Prerequisite: build cryptolib's C ABI once (produces `libcryptolib_c`):

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
cmake -S . -B build-asan -DBLOCKCHAINER_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan --output-on-failure

# libFuzzer codec harness (needs a libFuzzer-capable clang)
cmake -S . -B build-fuzz -DBLOCKCHAINER_BUILD_FUZZERS=ON
cmake --build build-fuzz --target fuzz_codec
./build-fuzz/fuzz_codec -max_total_time=60
```

## Example app — Flutter dashboard

[`examples/blockchainer_demo`](examples/blockchainer_demo) is a desktop Flutter app
that drives the whole stack through the C ABI: create a chain, send transactions,
produce blocks (BFT), watch a live event stream, read Prometheus metrics, verify a
light-client proof, and derive Ethereum/Bitcoin/Solana addresses. Build the dylib,
then `cd examples/blockchainer_demo && flutter run -d macos`.

A console version: `cd bridge/bindings/dart && dart run bin/node_demo.dart`.

## License

MIT (matching cryptolib).
