# Aviary JVM binding (Java / Kotlin)

A Java Foreign Function & Memory (FFM / Panama) binding over the Aviary C
ABI. Drives the full node - create a chain, accounts, transfers, produce blocks
(BFT), stream events, read metrics, verify light-client proofs - plus
EVM/Bitcoin/Solana interop. Callable from **Java and Kotlin**.

Requires JDK 22+ (FFM is stable since 22).

## Build & run (dev, against the host dylib)

```bash
# from this directory; build the native lib first: (cd ../../.. && cmake --build build)
javac src/aviary/*.java -d out
LIB="$PWD/../../../build/libaviary_c.dylib"

# Java
java --enable-native-access=ALL-UNNAMED -cp out -Daviary.path="$LIB" aviary.Main

# Kotlin
kotlinc Demo.kt -cp out -include-runtime -d demo.jar
java --enable-native-access=ALL-UNNAMED -cp out:demo.jar -Daviary.path="$LIB" DemoKt
```

The native library is resolved by `NativeLoader`: `-Daviary.path` / env
`AVIARY_DYLIB` (dev), else a bundled `/native/<os>-<arch>/` resource, else
the repo `build/` dir. The dev dylib's embedded rpath resolves `libcryptolib_c`
automatically; the shipped engines are self contained, one library per
platform.

## Surface

`Aviary` (AutoCloseable) -> `version`, `cryptoBackend`, `createNode`,
`ethAddress`/`btcAddress`/`solAddress`, `verifyStateProof`. `Aviary.Node`
-> `newAccount`, `credit`, `submitTransfer`, `produceBlock`, `height`/`balance`/
`nonce`/`mempoolSize`, `stateRoot`, `drainEvents`, `metrics`, `proveAccount`.

## Events, persistence, custody signing and networking

- Push events: `node.SetEventHook(func(json string))` (Go), `node.onEvent(cb)`
  (npm), `node.setEventHook(Consumer<String>)` (JVM), `node.setEventHook { }`
  (Swift) receive each chain event as one JSON object; a nil hook restores
  buffering for the drain call.
- The same events and metrics on an assembled chain and a networked node:
  `Chain` and `NetNode` expose drain events, set event hook and metrics in
  every binding. A chain reports block_finalized or block_rejected per submit;
  a networked node reports block_finalized per committed height and counts
  gossip received, relayed and dropped, timeouts fired and peers banned.
- Persistence: `Snapshot` / `RestoreNode` / `BlockAt` rebuild a node from one
  blob by replaying its blocks through consensus.
- Custody signing: `KeyStore.SignTransfer`, `Node.SubmitTransferFromStore` and
  `Node.SubmitTx` sign from the sealed store without the secret ever leaving
  the engine; `PublicKey`, `Has`, `Addresses`, `Lock` round out the store.
- Networking: `NetNode` runs BFT consensus over a gossip mesh with no socket
  and no clock; every input returns the relays to send and the timeouts to
  arm (`Start`, `OnGossip`, `OnTimeout`, `Advance`, `SetTransactions`,
  `ReplayLog`). `SecureSession` protects each link with the mutually
  authenticated post quantum hybrid handshake (`GenerateSecureIdentity`,
  `InitiateSecureSession`, `AcceptSecureSession`, `Encrypt`, `Decrypt`).
  Names follow each language's casing.

## Records, peering and admission

- Records: a chain built from a genesis with state model 2 stores 32 byte
  keys, each with an owner, a version and a value of up to 64 KiB.
  `chain.write(secret, key, op, value)` signs and queues a write with the
  nonce taken from the chain; the ops are `RecordOp.PUT`, `RecordOp.TRANSFER`
  and `RecordOp.DELETE`, and only the owner may update, transfer or delete.
  `chain.record`, `chain.recordNonce` and `chain.proveRecord` read the
  committed state, and `aviary.recordFromProof(proof)` reads the record out of
  a state proof that verified. `aviary.signRecordTx` signs a write without a
  chain handle.
- Custody and post quantum authors: `chain.writeFromStore(store, from, key,
  op, value)` and `keyStore.signRecord` sign record writes from the sealed key
  store, with the scheme taken from the stored key. `node.newAccountHybrid`,
  `node.submitTransferHybrid` and `aviary.signRecordTxHybrid` sign with
  Ed25519 and ML-DSA-65 together in the version 2 envelope.
- Networked records: on a record model network `netNode.record`,
  `netNode.recordNonce` and `netNode.proveRecord` answer on every node,
  observers included. A proposer includes the list given to `setTransactions`;
  the list is cleared when the height commits, so set the next height's list
  after the commit and before `advance`.
- In process meshes: `new Aviary.PeerMesh(nodes)` with `link`, `linkAll`,
  `start`, `advance`, `step` and `settle` relay in emission order and fire
  timeouts only when nothing is pending, for tests and single process
  networks.
- Peer exchange: `aviary.signPeerRecord`, `verifyPeerRecord` and
  `readPeerRecord` handle self signed peer records (id, host, port, KEM and
  signature keys, optional prekey). `aviary.newAddressBook()` /
  `decodeAddressBook(bytes)` give an `AddressBook` with `add`, `merge`,
  `record(id)`, `records` and `encode`; a merge verifies every record and
  never overwrites a known id.
- Noise links: `aviary.generateNoiseIdentity`, `initiateNoiseSession(me,
  prologue)` and `acceptNoiseSession(me, msg0, prologue)`; the session runs
  `handshakeMessage`, `completeInitiator` / `completeResponder`, exchanges
  `identityProof` / `verifyIdentityProof`, then `encrypt` and `decrypt`. Pin
  `peerIdentity` so a reconnecting peer cannot change keys.
- Admission: `gate.admissionChallenge(epoch)` gives a nonce to keep and a wire
  to send; the peer answers with `policy.admissionAnswer(credential, wire)`
  and the verifier checks it with `gate.admissionGranted(epoch, nonce, answer,
  now)`, so only credential holders get past the handshake.

## Subchains version 2 and chain persistence

- Dynamic registration: a parent whose genesis commits
  `aviary.encodeDynamicSubchainPolicy(policy)` (an `Aviary.DynamicSubchainPolicy`
  with its bounds, `Aviary.Registrar` keys and threshold) registers children
  in any block. `opRegister`, `opClose` and `opSetRegistrars` build unsigned
  registry operations; `signOp` (or `opDigest` plus `addOpApproval` for a
  signer outside the library) adds a registrar approval, and
  `chain.proposeSubchain(timestamp, anchors, ops)` proposes a block carrying
  them. Replacing the registrars increments the authority epoch, which voids
  every operation signed for the old one.
- Merkle registry: `chain.registryCounts()` (null without a dynamic policy),
  `chain.childRecord(id)` (an `Aviary.ChildRecord` with the child's status
  and recorded tip), `chain.childProof(id)` and
  `aviary.verifyChildProof(subchainRoot, proof)`, which returns the proven
  record. `aviary.pendingAnchor(parent, child, limit)` builds the next anchor
  from the child's archive, or returns null when nothing is pending.
  `chain.subchainHopV2`, `encodeSubchainProofV2` and `verifySubchainProofV2`
  prove state on a descendant through any mixture of parent formats.
- Chain persistence: `chain.snapshot()` and `aviary.restoreChain(genesis, me,
  blsPub, blsSec, snapshot)` resume an assembled chain that then produces byte
  for byte what the original would; `chainSnapshotError` says why a blob would
  be refused. A snapshot holds no secret key. `chain.enableArchive()` keeps
  every committed block with its certificate (`archiveRange`, `archivedBlock`,
  `archivedCertificate`, `pruneArchive`), and `chain.tip()` returns the tip
  header. `tree.snapshot()` and `aviary.restoreChainTree(root, snapshot)` do
  the same for a whole chain tree; hooks are not restored.
- `aviary.hash(bytes)` is BLAKE3-256 and `aviary.encodedHeaderHash(header)` the
  canonical hash of an encoded header.

`aviary.Main` replays `vectors/subchain_v2_vectors.txt` when it runs inside
the repository (or from `-Daviary.vectors=<file>`), so the JVM binding is held
to the same golden bytes as C++, Go and Dart.

## Self contained jar

```bash
bash scripts/jvm/build_jar.sh 1.0.0
java --enable-native-access=ALL-UNNAMED -jar dist/jvm/aviary-1.0.0.jar
```

The jar carries the compiled binding and the native engine for macOS arm64
and Linux x64 and arm64 under `/native/<os>-<arch>/`; `NativeLoader`
extracts the slot (every file in its `index.txt`, today one self contained
library with CipherBird linked in) into a temporary directory, and
`-Daviary.path` or `AVIARY_DYLIB` still override it. Attach the jar to a GitHub release or
publish it to a Maven repository with your own coordinates.
