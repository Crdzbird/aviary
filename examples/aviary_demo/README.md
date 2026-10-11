# Aviary dashboard (Flutter)

A desktop Flutter app that drives the entire engine directly through the
Dart binding, with no server: one screen showing every capability live.

## What it shows

- **Chain and accounts.** An in-memory single validator account chain with
  Alice funded at start.
- **Transactions and consensus.** Send from Alice to Bob, then produce and
  finalize a block in one BFT round. "Auto-mine" finalizes a block as soon as
  a transaction arrives.
- **Live event stream.** `BlockFinalized`, `TxAdmitted` and `TxRemoved`
  events arrive through the typed event stream as they happen.
- **Metrics.** Height, blocks, transactions and mempool size from the
  Prometheus text, updating live.
- **Prove and verify.** A light client state inclusion proof for Alice,
  verified against the chain's state root; a tampered proof fails.
- **Interop addresses.** Ethereum, Bitcoin and Solana addresses derived
  from Alice's key by the in-process interop layer.
- **Mixture demo.** Assembles PoS and PoW chains from blueprints and commits
  a block on each, then shows the fail closed rejects.
- **Subchain demo.** A BFT child anchored by a parent and a cross chain
  proof verified from the parent header.
- **Membership demo.** Anonymous credentials: issue, present, admit, the
  pseudonymous Sybil cap and revocation.

## Run

```bash
# 1. Build the native library (once, from the repository root)
( cd ../.. && cmake -S . -B build && cmake --build build )

# 2. Run on macOS desktop
flutter pub get && flutter run -d macos
```

The app resolves `aviary_dart` from this repository through
`pubspec_overrides.yaml` and finds `libaviary_c.dylib` in the repository's
`build/` directory, or wherever `AVIARY_DYLIB` points. The macOS sandbox is
disabled in this example so the development dylib loads by path; a shipped
app uses the `aviary_lite` or `aviary` plugin instead.

## How the screen maps to the library

| Section | Library |
|---|---|
| Chain and accounts | `Node`, `fundAccount`, `sendTo` |
| Produce block | `commit` (BFT round with a BLS aggregated certificate) |
| Live events | `eventStream` |
| Metrics | `metricValues` |
| Prove and verify | `proveAccount`, `verifyAccountProof` |
| Interop addresses | `ethAddress`, `btcAddress`, `solAddress` |
| Mixture demo | `makeBlueprint`, `commitBlueprint`, `assembleChain` |
| Subchain demo | `commitSubchainPolicy`, `proposeAnchored`, `verifySubchainProof` |
| Membership demo | `newBBSIssuer`, `makeCredentialPolicy`, `makeCredentialGate` |
