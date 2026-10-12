# Aviary

Aviary is a distributed ledger toolkit written in C++20 and exposed through
one C ABI, so the same engine runs from Dart, Flutter, Go, npm, JVM and
Swift. Consensus, state and verification live once, in C++; the bindings
only marshal bytes. Every cryptographic primitive comes from
[CipherBird](https://github.com/Crdzbird/CipherBird), including the post
quantum ones.

Use it to run a private settlement rail, anchor data on a chain a device
owns, govern a validator council, prove balances to a light client, or
connect a custom chain to Ethereum, Bitcoin and Solana. Everything below is
shown in Dart, the most direct binding; the other bindings expose the same
surface with the same names.

## Contents

1. [Pick a binding](#pick-a-binding)
2. [Five minute tour](#five-minute-tour)
3. [Feature guide](#feature-guide)
4. [Example apps](#example-apps)
5. [Build from source](#build-from-source)
6. [Repository layout](#repository-layout)
7. [Documentation](#documentation)

## Pick a binding

| You are writing | Use | Install |
|---|---|---|
| A Dart CLI, server or test | `aviary_dart` | `dependencies: aviary_dart: ^0.1.0` |
| A Flutter app that only needs the ledger | `aviary_lite` | `dependencies: aviary_lite: ^0.1.0` (one self-contained native binary) |
| A Flutter app that also calls CipherBird directly | `aviary` | `dependencies: aviary: ^0.1.0` (shares the `cipherbird` package's engine) |
| Go | `bridge/bindings/go` | `go get github.com/Crdzbird/aviary/bridge/bindings/go@bridge/bindings/go/v0.1.0` |
| Node.js | `aviary-ledger` | `npm install aviary-ledger` (bundles macOS arm64 and Linux engines) |
| Swift (iOS, macOS) | Swift package at the repository root | `.package(url: "https://github.com/Crdzbird/aviary", from: "0.1.0")` |
| Java or Kotlin (JDK 22+) | `bridge/bindings/jvm` | jar from `scripts/jvm/build_jar.sh` |
| Android without Flutter | `bridge/bindings/aviary-android` | AAR with the native pair and an off-thread preload |

Each binding directory has its own README with the full API and the
platform notes:
[Dart](bridge/bindings/dart/README.md),
[Go](bridge/bindings/go/README.md),
[npm](bridge/bindings/aviary-node/README.md),
[Swift](bridge/bindings/swift/README.md),
[JVM](bridge/bindings/jvm/README.md),
[Android](bridge/bindings/aviary-android/README.md).

## Five minute tour

A node is a chain, its accounts, a mempool and BFT consensus in one object.
Fund two accounts, move value, finalize a block, then prove the result to
someone who does not trust you:

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
  final proof = node.proveAccount(bob.address);
  print(node.verifyAccountProof(proof));
  print(node.stateRoot().hex);
  node.dispose();
}
```

`commit` runs a BFT round with BLS aggregated votes and finalizes the block
at once: there is no probabilistic settlement. `sendTo` picks the next nonce
itself, counting transfers still waiting in the mempool, so several
transfers from one account can be queued before a block. Every shortcut has
an explicit counterpart (`newAccount` and `credit`, `submitTransfer` with a
nonce, `produceBlock` with a timestamp) when you need full control.

## Feature guide

### Ledger and finality

- **Accounts or UTXOs.** The state model is chosen at genesis: an account
  model with balances and nonces, or a UTXO model with multisig outputs.
- **Instant finality.** Tendermint style rounds, deterministic proposer
  rotation, BLS aggregated commit certificates. A finalized block never
  reverts; two honest nodes cannot finalize conflicting blocks.
- **Hybrid post quantum signatures.** Accounts can sign with Ed25519 plus
  ML-DSA-65 (`fundHybridAccount`); the chain can require it.
- **Determinism everywhere.** A canonical byte encoding for every object,
  golden vectors in `vectors/` replayed by C++, Dart and Go, and fuzzed
  decoders. Identical inputs produce identical state roots in every binding.

### Composable chains

A chain is a mixture of three parts chosen at genesis: a consensus engine
(`bft`, `pos`, `vrf-pos`, `pow`), a state model (`account`, `utxo`) and a
proposer policy (`round-robin`, `stake-weighted`). The mixture is committed
into the genesis hash, so two chains that differ only in engine are distinct
chains. Every assembled chain follows one flow: propose, then seal (PoS) or
mine (PoW), then submit.

```dart
final validator = bc.newValidator();
final chain = bc.assembleSingleValidator(chainId: 9, validator: validator);
chain.produceBlock(1);
print(chain.height);
```

Foreign components let you write an engine or a proposer in the binding
language and register it under a name; the C++ core calls back into Dart,
Go or JavaScript to justify and verify blocks (`bc.registerEngine`,
`bc.registerProposer`). Postures are enforced: a hardened assurance level
refuses engines that are not grind proof, and a mixture whose state model
does not match the genesis is rejected before it can run.

### Subchains and chain trees

A parent chain registers child chains in its genesis policy and anchors
their finalized headers in its own blocks; the child commits a binding to
its parent. A cross chain proof then walks from a parent header down to an
account on the child, so anyone holding the parent's header can verify
state on any registered child (`encodeChildRegistration`,
`commitSubchainPolicy`, `commitParentBinding`, `proposeAnchored`,
`encodeSubchainProof`, `verifySubchainProof`).

A chain tree declares a whole family at once and drives it together:

```dart
final tree = bc.buildChainTree(TreeNode(
  chainId: 1,
  blueprint: pos,
  validators: [root],
  children: [
    TreeNode(chainId: 42, validators: [a, b, c], children: [
      TreeNode(chainId: 420, validators: [d], accounts: [(address: alice, balance: 1000)]),
    ]),
  ],
));
tree.produce(420, 2);
tree.produce(42, 2);
tree.produce(1, 2);
final proof = tree.proveAccount(420, alice);
print(tree.verify(proof));
```

Hooks run in the binding: `tree.onFinalized` observes every finalized block
and `tree.txSource` supplies the transactions a chain includes when it
produces.

### Validator rotation and light clients

A BFT chain schedules its next validator set (`scheduleValidators`); the
following block announces it and is certified by the current set, so the
change is itself finalized and the new set rules from the block after. A
light client follows the chain from a trusted genesis set without a full
node: `accept(header, certificate)` checks each block in sequence, and
`acceptSkipping(checkpoint)` jumps straight to a far header when that
header's own set certifies it and the signers hold more than a third of the
set the client already trusts (the overlap rule). When the overlap is too
thin, `syncTo` bisects through the rotation headers it fetches on demand.
`setMaxSkip` and `setTrustLevel` bound how far a client will jump.

### Networking

Consensus runs over a network the application owns. A `NetNode` is a
validator or an observer with no socket and no clock: every input returns an
`Outbound` listing the relays to send and the timeouts to arm, and the
application feeds what arrives back through `onGossip` and `onTimeout`.
Dart sockets, Flutter's connectivity rules and the app's retry policy stay in
Dart.

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
```

Each link is a `SecureSession`: a mutually authenticated, post quantum
hybrid channel (`generateSecureIdentity`, `initiateSecureSession`,
`acceptSecureSession`, then `encrypt` and `decrypt` per frame). Peers that
send garbage are scored and banned (`peerBanned`); a late peer catches up
from `replayLog`.

For tests, simulations and single process networks, `PeerMesh` is the whole
loop: relays are delivered in order and a timeout fires only when nothing is
pending, so the same inputs always reach the same commit.

```dart
final mesh = PeerMesh([
  for (var i = 0; i < 4; i++)
    bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
])..linkAll();
mesh.start();
mesh.settle();
mesh.advance();
mesh.settle();
print(mesh.nodes.first.height);
mesh.free();
```

### Custody

- **Key stores.** `createKeyStore(passphrase)` seals signing secrets at
  rest. `store.signTransfer(...)` and `node.sendFromStore(...)` sign inside
  the engine; the secret never crosses into Dart and is wiped after use.
  `lock()` when the app goes to the background, `unlock(passphrase)` when it
  returns, `loadKeyStore` to reopen a saved store.
- **Portable accounts.** Seal an account to a passphrase, to a recipient's
  hybrid KEM key or with a signature (`sealAccountPassphrase`,
  `openAccountPassphrase`, `openAccountTo`, `openAccountSignedFrom`) so it
  can move between devices. Async variants run off the UI isolate.
- **Threshold accounts.** `makeCeremony` runs a key generation ceremony,
  `createThresholdAccount` produces an account whose signature needs k of n
  shares, `sealShare` protects each share and `signWithShares` combines them
  into an ordinary Ed25519 signature the chain accepts.

### Assurance, sealed mempools and credentials

- **Assurance postures.** A genesis assurance level (`describeAssurance`)
  fixes what the chain tolerates: which engines qualify, whether hybrid
  signatures are mandatory, how proposers are elected.
- **Sealed mempools.** `makeSealedMempool` encrypts transactions until a
  block includes them, so a proposer cannot front run what it has not yet
  committed to (AES-256-GCM-SIV under a committing cascade).
- **Anonymous membership credentials.** An issuer (`newBBSIssuer`) grants
  BBS credentials with a tier and an expiry under a `CredentialPolicy`. A
  member presents one per epoch and scope; a `CredentialGate` admits it
  without learning who the member is, and two presentations of one
  credential are unlinkable. The pseudonymous mode derives one stable
  pseudonym per member, epoch and scope, which caps Sybil attempts and
  allows targeted revocation (`revokeNym`) without identifying anyone.
  Blind issuance keeps the member secret away from the issuer.

### Application records

Not every chain moves money. A chain assembled with the `record` state model
stores application objects instead of balances: a 32 byte key chosen by the
application, an owner, a version that grows by one per write, and an opaque
value of up to 64 KiB (JSON, protobuf, CBOR, a content hash). Only the owner
may update, transfer or delete a record, replay protection is a per author
nonce, and consensus, proofs, snapshots and events work exactly as they do
for balances.

```dart
final chain = bc.assembleRecordChain(chainId: 31, validator: bc.newValidator());
final key = bc.hash(utf8.encode('asset:PUMP-0042'));
chain.write(facilities.secret, key, RecordOp.put, utf8.encode('{"status":"in service"}'));
chain.produceBlock(1);
final record = chain.record(key)!;            // owner, version 1, the JSON bytes
chain.write(facilities.secret, key, RecordOp.transfer, lab.address);
chain.produceBlock(2);                         // the lab owns it now

final proof = chain.proveRecord(key);          // for someone who only trusts the root
bc.verifyStateProof(chain.stateRoot(), proof);
print(bc.recordFromProof(proof).version);
```

`addTx` queues any signed transaction on any assembled chain, so the same
path carries hand built transactions for the account and UTXO models too.
The [Registry example](examples/registry) runs a whole equipment registry
this way. Custom models beyond records are a C++ `StateMachine` subclass
registered in the component registry under its own id.

### Observability

Nodes, assembled chains and networked nodes all expose typed events and
Prometheus metrics. `events()` drains a buffer; `eventStream` is a broadcast
stream fed by a native hook, so nothing is polled.

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
print(node.metricValues()['aviary_chain_height']);
```

A networked node adds counters for gossip received, relayed and dropped,
timeouts fired and peers banned.

### Persistence

`node.snapshot()` captures the whole chain as one blob: chain id, genesis
credits, every finalized block with its certificate and the live ledger
state. `restoreNode(bytes)` resumes from the tip at once however long the
chain is; `restoreNode(bytes, replay: true)` replays every block through
consensus to audit the archive. `saveTo(path)` and `restoreNodeFrom(path)`
do the file handling.

```dart
final path = '${appDocuments.path}/chain.bin';
final node = await bc.restoreNodeFrom(path) ?? bc.createNode(7);
node.fundAccount(1000);
node.commit();
await node.saveTo(path);
```

### Interop with other chains

The same keys that sign on an Aviary chain derive addresses on Ethereum,
Bitcoin and Solana (`ethAddress`, `btcAddress`, `solAddress`). The engine
builds and signs real transactions for each: EIP-155 Ethereum transactions
and ERC-20 transfers with ABI encoding and `ecrecover`, Bitcoin P2PKH and
P2WPKH with BIP143 SegWit signing, Solana transfers in the native message
format, plus JSON-RPC clients over a transport you supply. All of it is
validated against real on-chain vectors. A federated bridge (BLS attested
lock-mint and burn-release) connects an Aviary chain to those networks.

## Example apps

Each example is a real scenario. Atlas, Sentinel and Interop pair a Go gRPC
server over the Go binding with a Flutter client; the dashboard drives the
engine directly from Flutter; the registry is a pure Dart program. Build the host library first from the repository root:

```bash
cmake -S . -B build && cmake --build build
```

| Example | Scenario | Run |
|---|---|---|
| [Atlas](examples/atlas) | Wholesale settlement network: per currency BFT rails, institutional wallets with cross chain receive addresses, issuance and settlement with finality, balance proofs, signed ERC-20 transfers, a validator council with an auditor light client | `cd examples/atlas && ./run.sh` |
| [Sentinel](examples/sentinel) | Device provenance ledger: a chain born from a device's enrollment, content hashes of photos, video, GPS and sensor data anchored with finality, proofs that data existed unaltered, a fleet root anchoring one subchain per device | `cd examples/sentinel && ./run.sh` |
| [Interop](examples/interop) | Go and Flutter run the same ledger independently and cross check canonical bytes, state roots, light client proofs and a chain tree in both directions | `cd examples/interop && ./run.sh` |
| [Registry](examples/registry) | An equipment registry on a record chain, no money involved: JSON documents as records, signed writes finalized in blocks, ownership transfer, versions, an auditor verifying a record from the state root alone, deletion | `cd examples/registry && dart run bin/registry.dart` |
| [Dashboard](examples/aviary_demo) | One Flutter screen driving every capability live: accounts, transactions, blocks, the event stream, metrics, proofs, interop addresses, and the mixture, subchain and membership demos | `cd examples/aviary_demo && flutter run -d macos` |

`run.sh` starts the server and the Flutter client on macOS; pass
`-d <device>` for an iOS simulator or an Android device (the READMEs
explain the `adb reverse` step for Android). Sentinel and Interop also have a
`probe` command under `server/cmd` that runs the scenario without Flutter.

Console demos without Flutter: `dart run bin/node_demo.dart` in
`bridge/bindings/dart`, `go run ./cmd/demo` in `bridge/bindings/go`, and
`swift run AviaryDemo` from the repository root.

## Build from source

Prerequisite: CipherBird's C ABI, built once from the sibling checkout.

```bash
cd ../cryptolib && bash scripts/build_selfcontained.sh
```

Then build and test the whole stack:

```bash
bash scripts/build_and_test.sh
```

Or step by step:

```bash
cmake -S . -B build && cmake --build build
ctest --test-dir build --output-on-failure
( cd bridge/bindings/dart && dart pub get && dart test )
( cd bridge/bindings/go && CGO_ENABLED=1 go test ./aviary/ )
```

Hardening builds: `-DAVIARY_SANITIZE=ON` for AddressSanitizer and UBSan,
`-DAVIARY_SANITIZE_THREAD=ON` for ThreadSanitizer, and
`-DAVIARY_BUILD_FUZZERS=ON` for the libFuzzer harnesses over the codec, the
snapshot decoder and the C ABI. Distribution builds default to link time
optimization and dead section elimination; the shipped engines are 7 MB on
Linux and 6 MB on Android.

Native artifacts for every platform, the two commit procedure for C ABI
changes and the publishing steps are in [`docs/MOBILE.md`](docs/MOBILE.md).

## Repository layout

```
include/aviary/          public C++ headers
src/                     C++ core: codec, state, consensus, p2p, client, custody, interop
bridge/aviary_c.{h,cpp}  the C ABI every binding marshals over
bridge/bindings/dart     aviary_dart (pub package)
bridge/bindings/aviary_lite, aviary   the two Flutter plugins
bridge/bindings/go, aviary-node, jvm, swift, aviary-android
examples/                Atlas, Sentinel, Interop, the Flutter dashboard, the registry
tests/                   69 C++ suites (ctest)
fuzz/                    libFuzzer harnesses
vectors/                 golden vectors replayed by every binding
scripts/                 native builds per platform, release tooling, CI entry points
docs/                    design, codec spec, threat model, mobile builds, backlog
```

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md): the design and the
  decisions behind it.
- [`docs/CODEC_SPEC.md`](docs/CODEC_SPEC.md): the canonical byte encoding.
- [`docs/THREAT_MODEL.md`](docs/THREAT_MODEL.md): what the engine defends
  against and how.
- [`docs/design/SUBCHAINS.md`](docs/design/SUBCHAINS.md) and
  [`docs/design/CREDENTIAL_GATE.md`](docs/design/CREDENTIAL_GATE.md): the
  two largest feature designs.
- [`docs/MOBILE.md`](docs/MOBILE.md): native artifacts, Flutter packaging,
  publishing.
- [`docs/CONSUMER_API_README.md`](docs/CONSUMER_API_README.md): a Go
  service skeleton over the binding.
- [`docs/IMPROVEMENTS.md`](docs/IMPROVEMENTS.md): the running backlog and
  the record of what shipped.

## License

MIT, matching CipherBird.
