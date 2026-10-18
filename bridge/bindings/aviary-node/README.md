# aviary (Node.js)

Aviary for Node.js via [koffi](https://koffi.dev) FFI - a full DLT (ledger,
BFT consensus, mempool, observability, light-client verification) plus
EVM/Bitcoin/Solana interop.

```js
const bc = require('aviary');

bc.preload();                       // optional: warm the native lib off-thread
                                    // (worker_thread). The API never needs await.

const node = bc.newNode(1n);
const a = node.newAccount();
const b = node.newAccount();
node.credit(a.address, 1_000_000n);
node.submitTransfer(a.secret, b.address, 250_000n, node.nonce(a.address));
node.produceBlock(1n);              // -> 1  (finalized)
node.balance(b.address);           // -> 250000

// Light-client proof
bc.verifyStateProof(node.stateRoot(), node.proveAccount(b.address)); // true

// EVM smart contract
bc.functionSelector('transfer(address,uint256)').toString('hex');    // a9059cbb
bc.signLegacyTx({ to, value: 0n, data: bc.erc20TransferCalldata(to, 1n), secret });
```

## Preload (off the main thread, no await)

`preload()` spawns a short-lived `worker_thread` that loads the shared libraries
and runs the one-time crypto-backend init. Because `dlopen` mapping + backend
init are process-global, this warms the path for the main thread's later lazy
load - the first synchronous call then pays no load cost. It is **optional and
fire-and-forget**: the synchronous API works identically whether or not you call
it, and never requires `await`.

## Native library

The native pair (`libaviary_c` + `libcryptolib_c`) ships under
`prebuilds/<platform>-<arch>/`, made relocatable so they load with no setup.
Rebuild + restage with `scripts/node/stage_prebuild.sh`. For development you can
point at any build with `AVIARY_DYLIB` / `CRYPTOLIB_DYLIB`.

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
  committed state, and `recordFromProof(proof)` reads the record out of a
  state proof that verified. `signRecordTx` signs a write without a chain
  handle.
- Custody and post quantum authors: `chain.writeFromStore(store, from, key,
  op, value)` and `keyStore.signRecord` sign record writes from the sealed key
  store, with the scheme taken from the stored key. `node.newAccountHybrid`,
  `node.submitTransferHybrid` and `signRecordTxHybrid` sign with Ed25519 and
  ML-DSA-65 together in the version 2 envelope.
- Networked records: on a record model network `netNode.record`,
  `netNode.recordNonce` and `netNode.proveRecord` answer on every node,
  observers included. A proposer includes the list given to `setTransactions`;
  the list is cleared when the height commits, so set the next height's list
  after the commit and before `advance`.
- In process meshes: `new PeerMesh(nodes)` with `link`, `linkAll`, `start`,
  `advance`, `step` and `settle(validators)` relay in emission order and fire
  timeouts only when nothing is pending, for tests and single process
  networks.
- Peer exchange: `signPeerRecord`, `verifyPeerRecord` and `readPeerRecord`
  handle self signed peer records (id, host, port, KEM and signature keys,
  optional prekey). `new AddressBook(encoded?)` gives a book with `add`,
  `merge`, `record(id)`, `records` and `encode`; a merge verifies every record
  and never overwrites a known id.
- Noise links: `generateNoiseIdentity`, `NoiseSession.initiate(me, prologue)`
  and `NoiseSession.accept(me, msg0, prologue)`; the session runs
  `handshakeMessage`, `completeInitiator` / `completeResponder`, exchanges
  `identityProof` / `verifyIdentityProof`, then `encrypt` and `decrypt`. Pin
  `peerIdentity` so a reconnecting peer cannot change keys.
- Admission: `gate.admissionChallenge(epoch)` gives a nonce to keep and a wire
  to send; the peer answers with `policy.admissionAnswer(credential, wire)`
  and the verifier checks it with `gate.admissionGranted(epoch, nonce, answer,
  now)`, so only credential holders get past the handshake.

## Install

```bash
npm install aviary-ledger
```

The package bundles the native engine for macOS arm64 and Linux x64 and
arm64 under `prebuilds/`; `require('aviary-ledger')` finds it with no
configuration. `AVIARY_DYLIB` still overrides the path for development
builds. Releases are published from tags `aviary-ledger-vX.Y.Z` by the
`Publish to pub.dev` workflow through npm trusted publishing.
