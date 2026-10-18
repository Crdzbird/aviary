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

## Self contained jar

```bash
bash scripts/jvm/build_jar.sh 0.1.0
java --enable-native-access=ALL-UNNAMED -jar dist/jvm/aviary-0.1.0.jar
```

The jar carries the compiled binding and the native engine for macOS arm64
and Linux x64 and arm64 under `/native/<os>-<arch>/`; `NativeLoader`
extracts the slot (every file in its `index.txt`, today one self contained
library with CipherBird linked in) into a temporary directory, and
`-Daviary.path` or `AVIARY_DYLIB` still override it. Attach the jar to a GitHub release or
publish it to a Maven repository with your own coordinates.
