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
