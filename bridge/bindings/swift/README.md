# Aviary for Swift

The Swift binding wraps the Aviary C ABI: nodes, accounts and transfers,
composable chains, subchains, light clients, chain trees, custody (key stores,
portable and threshold accounts, key store backed signing), node snapshots,
the push event hook, networking (`NetNode` over an application owned
transport, `SecureSession` links) and the EVM, Bitcoin and Solana interop
helpers.

## Install

The package manifest is `Package.swift` at the repository root, so a
dependency is one line:

```swift
.package(url: "https://github.com/Crdzbird/aviary", from: "1.0.1"),
```

with the product `Aviary`. The two binary targets, `AviaryC.xcframework` and
the merged `CipherBird.xcframework`, download from the GitHub release named in
the manifest and are verified by checksum. Platforms: iOS 15 and macOS 12.
Version 1.0.0's manifest asked for macOS 14 although its binaries run on
macOS 12; 1.0.1 corrects that and ships the same binaries.

## Records, peering and admission

- Records: a chain built from a genesis with state model 2 stores 32 byte
  keys, each with an owner, a version and a value of up to 64 KiB.
  `chain.write(secret:key:op:value:)` signs and queues a write with the nonce
  taken from the chain; the ops are `.put`, `.transfer` and `.delete`, and
  only the owner may update, transfer or delete. `chain.record(key:)`,
  `chain.recordNonce(address:)` and `chain.proveRecord(key:)` read the
  committed state, and `Aviary.recordFromProof(_:)` reads the record out of a
  state proof that verified. `Aviary.signRecordTx` signs a write without a
  chain handle.
- Custody and post quantum authors:
  `chain.writeFromStore(store:from:key:op:value:)` and `keyStore.signRecord`
  sign record writes from the sealed key store, with the scheme taken from the
  stored key. `node.newAccountHybrid()`, `node.submitTransferHybrid` and
  `Aviary.signRecordTxHybrid` sign with Ed25519 and ML-DSA-65 together in the
  version 2 envelope.
- Networked records: on a record model network `netNode.record(key:)`,
  `netNode.recordNonce(address:)` and `netNode.proveRecord(key:)` answer on
  every node, observers included. A proposer includes the list given to
  `setTransactions`; the list is cleared when the height commits, so set the
  next height's list after the commit and before `advance()`.
- In process meshes: `Aviary.PeerMesh(nodes)` with `link`, `linkAll`, `start`,
  `advance`, `step` and `settle` relay in emission order and fire timeouts
  only when nothing is pending, for tests and single process networks.
- Peer exchange: `Aviary.signPeerRecord`, `verifyPeerRecord` and
  `readPeerRecord` handle self signed peer records (id, host, port, KEM and
  signature keys, optional prekey). `Aviary.AddressBook(encoded:)` gives a
  book with `add`, `merge`, `record(id:)`, `records()` and `encode()`; a merge
  verifies every record and never overwrites a known id.
- Noise links: `Aviary.generateNoiseIdentity()`,
  `initiateNoiseSession(me:prologue:)` and
  `acceptNoiseSession(me:msg0:prologue:)`; the session runs
  `handshakeMessage`, `completeInitiator` / `completeResponder`, exchanges
  `identityProof` / `verifyIdentityProof`, then `encrypt` and `decrypt`. Pin
  `peerIdentity` so a reconnecting peer cannot change keys.
- Admission: `gate.admissionChallenge(epoch:)` gives a nonce to keep and a
  wire to send; the peer answers with
  `policy.admissionAnswer(credential:wire:)` and the verifier checks it with
  `gate.admissionGranted(epoch:nonce:presentation:now:)`, so only credential
  holders get past the handshake.

## Dynamic subchains and chain snapshots

Unreleased: these calls need an `AviaryC` built from this branch, not the
`swift-v1.0.0` binaries.

- Dynamic policy: `Aviary.DynamicSubchainPolicy(threshold:registrars:)` with
  `Aviary.Registrar(scheme:publicKey:)`, committed with
  `Aviary.commitSubchainPolicy`. A parent then registers, closes and rotates
  registrars in any block through signed operations:
  `Aviary.opRegister`, `opClose` and `opSetRegistrars` build them,
  `Aviary.signOp` (or `opDigest` plus `addOpApproval` for a signer elsewhere)
  approves them, and `chain.proposeSubchain(timestamp:anchors:ops:)` includes
  them. Replacing the registrars bumps `chain.registryCounts()?.authorityEpoch`
  and voids operations signed for the old epoch.
- Anchoring: `Aviary.pendingAnchor(parent:child:)` builds the next anchor from
  the child's archive (`chain.enableArchive()`), nil when nothing is pending.
- Child proofs: `chain.childRecord(_:)` reads the 230 byte
  `Aviary.ChildRecord`, `chain.childProof(_:)` proves it against the parent's
  subchain root, and `Aviary.verifyChildProof(subchainRoot:proof:)` returns the
  proven record. `chain.subchainHopV2(_:)`, `Aviary.encodeSubchainProofV2` and
  `Aviary.verifySubchainProofV2` prove state on a child from a parent header
  through either policy format.
- Persistence: `chain.snapshot()` and `Aviary.restoreChain(genesis:me:blsPub:blsSec:snapshot:)`
  resume a chain that continues byte for byte; `Aviary.chainSnapshotError`
  says why a snapshot would be refused. `chain.tip()`, `archiveRange()`,
  `archivedBlock(_:)`, `archivedCertificate(_:)` and `pruneArchive(below:)`
  read and trim the block archive. `chainTree.snapshot()` and
  `Aviary.restoreChainTree(_:snapshot:)` do the same for a whole tree.

The demo replays `vectors/subchain_v2_vectors.txt`, the bytes the Go and Dart
bindings pin.

## Develop

Inside this repository the manifest uses the xcframework trees in the tree
when `AVIARY_LOCAL_XCFRAMEWORKS` is set:

```bash
bash scripts/apple/vendor_cipherbird.sh
AVIARY_LOCAL_XCFRAMEWORKS=1 swift run AviaryDemo
```

`scripts/apple/package_swift_release.sh swift-vX.Y.Z` zips both xcframeworks
into `dist/swift/`, computes the checksums and writes them into the manifest;
attach the zips to the GitHub release of that name. SwiftPM resolves the
version from a plain `X.Y.Z` tag, so also tag the commit that carries those
checksums with `X.Y.Z`; the `swift-vX.Y.Z` release only hosts the assets.
