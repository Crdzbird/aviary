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
- **Chain tree demo.** A whole family of chains declared at once, with
  anchors flowing upward and a grandchild proven from the root header.
- **Dynamic subchain demo.** Subchains version 2: a registrar signed
  operation registers a child at runtime, the parent anchors it, and both
  the child's registry record and a record written on the child are proven
  from the parent header.
- **Persistence demo.** A chain snapshot is checked, a tampered one is
  refused, and the restored chain proposes the same next block as the
  original. The block archive is read and pruned, and the dashboard's own
  node is restored from a snapshot.
- **Sealing demo.** An account and a threshold share card sealed under a
  passphrase with Argon2id. A wrong passphrase fails closed, and a memory
  cost that is not whole KiB is refused (CipherBird 1.6.0).

The title bar shows the engine version and the crypto backend, which ends
with the CipherBird revision the engine was built from.

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
| Chain tree demo | `TreeNode`, `buildChainTree`, `ChainTree.verify` |
| Dynamic subchain demo | `encodeDynamicSubchainPolicy`, `signOp`, `proposeSubchain`, `verifyChildProof`, `verifySubchainProofV2` |
| Persistence demo | `Chain.snapshot`, `chainSnapshotError`, `restoreChain`, `archiveRange`, `pruneArchive`, `restoreNode` |
| Sealing demo | `sealAccountPassphrase`, `openAccountPassphrase`, `sealShare`, `unsealShare` |

The newer cards live in `lib/demos/`, one scenario per file, and
`test/demos_test.dart` runs each of them against the engine with
`flutter test`.
