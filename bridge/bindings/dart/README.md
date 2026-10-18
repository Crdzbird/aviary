# aviary_dart

The Dart API of Aviary: a C++ ledger with BFT finality, composable
chains, subchains, validator rotation, light clients and chain trees, driven
over dart:ffi. Cryptography comes from the CipherBird engine.

Flutter apps should depend on `aviary` (shares the engine with the
`cipherbird` package) or `aviary_lite` (self contained) instead; both
re-export this API.

## Install

```yaml
dependencies:
  aviary_dart: ^0.1.0
```

The engine for macOS arm64 ships inside the package and is found through the
package root. Linux binaries under `native/linux-x64` and `native/linux-arm64`
are bundled by the package's build hook (`hook/build.dart`), so a Dart or
Flutter application on Linux gets `libaviary_c.so` next to its executable
with no setup. The hook also bundles `native/windows-x64/aviary_c.dll` when
that file exists; no Windows binary ships yet because the CipherBird engine
has no Windows build. Anywhere else, point `AVIARY_DYLIB` at a built
`libaviary_c` library. `Aviary.loadedFrom` reports which path won.

## Use

```dart
import 'package:aviary_dart/aviary.dart';

void main() {
  final bc = Aviary.instance;
  final node = bc.createNode(7);

  final alice = node.fundAccount(1000);
  final bob = node.fundAccount(0);
  node.sendTo(alice, bob, 250);
  node.commit();

  print(node.balance(bob.address));
  print(node.verifyAccountProof(node.proveAccount(bob.address)));
  print(node.stateRoot().hex);
  node.dispose();
}
```

Every call above has an explicit counterpart (`newAccount` plus `credit`,
`submitTransfer` with a nonce, `produceBlock` with a timestamp) for code
that needs full control. `sendTo` takes its nonce from `nextNonce`, which
counts transfers still waiting in the mempool, so several transfers from one
account can be queued before a block.

## Shortcuts

Extensions that collapse the common flows into one call:

| On | Members |
|---|---|
| `Uint8List` | `hex`, `shortHex`, `equalsBytes` |
| `String` | `hexBytes` (with or without 0x), `utf8Bytes` |
| `Account` | `addressHex`, `isHybrid` |
| `Node` | `fundAccount`, `fundHybridAccount`, `send`, `sendTo` (nonce from `nextNonce`, scheme follows the account), `sendFromStore`, `submitTx`, `submitTransferFromStore`, `commit`, `events`, `eventStream`, `metricValues`, `verifyAccountProof`, `snapshot`, `blockAt`, `saveTo` |
| `Aviary` | `newValidator`, `assembleSingleValidator`, `assembleRecordChain`, `signRecordTx`, `addressOf`, `recordFromProof`, `signWithShares`, `restoreNode`, `restoreNodeFrom`, `newNetNode`, `generateSecureIdentity`, `initiateSecureSession`, `acceptSecureSession`, `withNode`, `withKeyStore`, `withTree`, `sealShareAsync`, `unsealShareAsync`, `sealAccountPassphraseAsync`, `openAccountPassphraseAsync` |
| `KeyPairBytes` | `entry(power)`, `treeValidator(power)` |
| `Chain` | `produceBlock(timestamp, justify:)`, `events`, `eventStream`, `metrics`, `metricValues`, `addTx`, `write`, `record`, `proveRecord`, `recordNonce`, `pendingCount` |
| `NetNode` | `events`, `eventStream`, `metrics`, `metricValues` |
| `TreeNode` | `withChild`, `find`, `chainIds` |
| `KeyStore` | `signTransfer`, `publicKey`, `has`, `addresses`, `lock` |
| `ChainTree` | `produceRound` |

```dart
final validator = bc.newValidator();
final chain = bc.assembleSingleValidator(chainId: 9, validator: validator);
chain.produceBlock(1);

final root = TreeNode(chainId: 1, validators: [validator.treeValidator(1)])
    .withChild(TreeNode(chainId: 2, validators: [validator.treeValidator(1)]));
final tree = bc.buildChainTree(root);
tree.produceRound(1);

final shares = bc.createThresholdAccount(3, 2);
final signature = bc.signWithShares([shares[0], shares[2]], 'pay'.utf8Bytes);
```

## Surfaces

- `Node`: a single validator chain; accounts, transfers, blocks, proofs.
- Mixtures: `makeBlueprint`, `buildGenesis`, `assembleChain`, `Chain`.
- Foreign components: `registerEngine`, `registerProposer`.
- Subchains: registrations, policies, anchors, `verifySubchainProof`.
- Validator rotation: `Chain.scheduleValidators`, `Chain.validators`.
- Light clients: `newLightClient`, `acceptSkipping`, `syncTo`.
- Networking: `newNetNode`, `NetNode`, `PeerMesh`, `SecureSession`,
  `generateSecureIdentity`, `signPeerRecord`, `AddressBook`,
  `NoiseSession`, `generateNoiseIdentity`, and the admission handshake on
  `CredentialGate` and `CredentialPolicy`.
- Records: `assembleRecordChain`, `ChainRecords` (`write`, `record`,
  `proveRecord`, `addTx`), `RecordOp`, `recordFromProof`.
- Chain trees: `TreeNode`, `buildChainTree`, `ChainTree`.
- Custody: `KeyStore`, portable accounts, threshold accounts, `Ceremony`.
- Assurance and sealed mempools; anonymous membership credentials.

## Records

A chain does not have to carry money. `assembleRecordChain` builds a chain
whose state is a signed key value store: each record has a 32 byte key the
application chooses, an owner, a version and an opaque value of up to 64 KiB.
`chain.write(secret, key, op, value)` signs and queues a write (the nonce
comes from the chain, so several writes can be queued before a block);
`produceBlock` finalizes them. Only the owner may `put`, `transfer` or
`delete`. `record(key)` reads the committed record, `proveRecord(key)` proves
it to someone who holds only the state root, and `recordFromProof` reads it
back out of a verified proof.

```dart
final chain = bc.assembleRecordChain(chainId: 31, validator: bc.newValidator());
final key = bc.hash(utf8.encode('asset:PUMP-0042'));
chain.write(owner.secret, key, RecordOp.put, utf8.encode('{"status":"in service"}'));
chain.produceBlock(1);
final proof = chain.proveRecord(key);
assert(bc.verifyStateProof(chain.stateRoot(), proof));
print(bc.recordFromProof(proof).version); // 1
```

`writeFromStore(store, author, key, op, value)` signs inside the engine from
a `KeyStore`, and `signRecordTxHybrid` signs with a hybrid post quantum
account. `addTx` queues any signed, encoded transaction on any assembled
chain, for the account and UTXO models as well, and each queued and included
transaction arrives as a `TxAdmitted` and `TxRemoved` event.

## Events

Chain events are typed. `node.events()` drains the buffer as a
`List<ChainEvent>`; `node.eventStream` is a broadcast `Stream<ChainEvent>`
fed by a native hook, so nothing is polled. `Chain` and `NetNode` carry the
same `events()`, `eventStream`, `metrics()` and `metricValues()`: a chain
reports a `BlockFinalized` or `BlockRejected` per `submit`, a networked node a
`BlockFinalized` per committed height plus gossip, timeout and ban counters.
The sealed `ChainEvent` has five subtypes, so a switch is exhaustive:

```dart
node.eventStream.listen((event) {
  switch (event) {
    case BlockFinalized(:final height, :final txCount):
      print('block $height with $txCount txs');
    case TxAdmitted(:final txid):
      print('pending ${txid.shortHex}');
    case TxRemoved():
    case BlockRejected():
    case UnknownEvent():
      break;
  }
});
```

Listeners run on the microtask after the node call that caused the event,
so they may call the node again. Disposing the node closes the stream.

## Custody signing

A `KeyStore` seals signing secrets at rest. Signing from it keeps the secret
inside the engine: `store.signTransfer(...)` returns an encoded transaction,
`node.sendFromStore(store, from, to, amount)` signs and admits in one call,
and `node.submitTx(bytes)` admits anything already signed. The scheme follows
the stored key, Ed25519 or hybrid post quantum, and the secret is wiped after
use. Lock the store with `lock()` when the app goes to the background and
`unlock(passphrase)` when it returns. `reveal` still exists for migrations,
but nothing in the signing path needs it.

```dart
final store = bc.createKeyStore(passphrase);
final alice = store.addAccount(account.publicKey, account.secret);
node.sendFromStore(store, alice, bob.address, 250);
net.setTransactions([
  store.signTransfer(chainId: 9, from: alice, to: bob.address, amount: 4, nonce: 0),
]);
```

## Networking

Consensus runs over a network the app owns. A `NetNode` is a validator (or
an observer) running BFT over a gossip mesh with no socket and no clock:
every input returns an `Outbound` with the relays to send and the timeouts
to arm, and the app feeds what arrives back in. Dart sockets, Flutter's
connectivity rules and the app's own retry policy stay in Dart.

```dart
final node = bc.newNetNode(genesis: genesis, me: 0, blsKey: myKey, peers: [1, 2]);
void apply(Outbound out) {
  for (final relay in out.relays) {
    sockets[relay.to].add(frame(sessions[relay.to].encrypt(relay.message)));
  }
  for (final t in out.timeouts) {
    Timer(Duration(milliseconds: 500 * (t.round + 1)), () => apply(node.onTimeout(t)));
  }
}
apply(node.start());
// on a frame from peer p: apply(node.onGossip(p, sessions[p].decrypt(frame)));
// once node.committed: apply(node.advance());
```

For tests, simulations and a single process network, `PeerMesh` is that
loop: it delivers relays in order and fires a timeout only when no relay is
pending, earliest round first, so the same inputs always reach the same
commit. `settle` runs until the validators commit and then drains the mesh
so observers catch up.

```dart
final mesh = PeerMesh([
  for (var i = 0; i < 4; i++)
    bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
])..linkAll();
mesh.start();
mesh.settle();
mesh.advance();
mesh.settle();
print(mesh.nodes.first.height); // 2
mesh.free();
```

Each link is protected by a `SecureSession`: a mutually authenticated, post
quantum hybrid channel. The dialer calls `initiateSecureSession` with the
peer's KEM public key, the listener `acceptSecureSession` with its own
`generateSecureIdentity()`, the two handshake messages cross, then `encrypt`
and `decrypt` wrap every frame. Always compare `peerIdentity` with the key
you expected. Blocks carry the encoded transactions passed to
`setTransactions` when this node is the proposer. The list is cleared when
the height commits, so hand over the next height's transactions after the
commit and before `advance`. `replayLog` catches up a peer that connects
after consensus started.

On a record model network (`buildGenesis(stateModel: 2)`) every node answers
`record(key)`, `recordNonce(address)` and `proveRecord(key)`, observers
included; `examples/registry/bin/network.dart` runs a registry that way on
four validators and an auditor.

## Persistence

A node's whole chain travels as one blob: `node.snapshot()` holds the chain
id, the genesis credits and every finalized block with its certificate.
`bc.restoreNode(bytes)` rebuilds the node from the ledger state the snapshot
carries and resumes from the tip at once, however long the chain is;
`restoreNode(bytes, replay: true)` replays every block through consensus
instead, verifying each certificate, which audits the archive at the cost of
startup time. Both yield the same node, byte for byte. `saveTo(path)` writes the blob atomically and
`restoreNodeFrom(path)` reads it back, returning null when no file exists.
Pending mempool transactions are not part of a snapshot. `blockAt(height)`
returns any archived block.

```dart
final path = '${appDocuments.path}/chain.bin';
final node = await bc.restoreNodeFrom(path) ?? bc.createNode(7);
node.fundAccount(1000);
node.commit();
await node.saveTo(path);
```

## Lifecycle and errors

Handles (`Node`, `Chain`, `KeyStore`, `LightClient`, `Ceremony`,
`SealedMempool`, `CredentialGate`, `ChainTree`) own native memory. Release
them with `dispose` or `free`, or let the garbage collector do it: every
handle has a finalizer, releasing twice is harmless, and `isReleased` tells
you where you stand. The `withNode`, `withKeyStore` and `withTree` helpers
release at the end of a scope. Errors are `AviaryException` with a
`status` from the C ABI (`AviaryStatus.genesisSealed`, `badInput`,
`failure`). The Argon2 bound custody calls have `Async` variants that run
on a worker isolate.

## Testing

```bash
dart test
```

The suites run one at a time (`dart_test.yaml`), because each drives the
same native engine from its own isolate. The GitHub workflow `Dart packages`
runs format, analysis, the suites and the pub.dev score on macOS and Linux
for `aviary_dart`, and analysis plus tests for the two Flutter packages.
Releases are published from tags (`aviary_dart-v0.1.0`, `aviary_lite-v0.1.0`,
`aviary-v0.1.0`) by the `Publish to pub.dev` workflow through pub.dev's
GitHub Actions trust; see that workflow's header for the pub.dev settings.

## Determinism

Every canonical byte string (hashes, transaction ids, headers, proofs) is
identical across the Go, npm, JVM, Swift and Dart bindings.
