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

The engine for macOS arm64 ships inside the package. On other hosts point
`AVIARY_DYLIB` at a built `libaviary_c` library.

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
that needs full control.

## Shortcuts

Extensions that collapse the common flows into one call:

| On | Members |
|---|---|
| `Uint8List` | `hex`, `shortHex`, `equalsBytes` |
| `String` | `hexBytes` (with or without 0x), `utf8Bytes` |
| `Account` | `addressHex`, `isHybrid` |
| `Node` | `fundAccount`, `fundHybridAccount`, `send`, `sendTo` (auto nonce, scheme follows the account), `commit`, `events`, `metricValues`, `verifyAccountProof` |
| `Aviary` | `newValidator`, `assembleSingleValidator`, `signWithShares`, `withNode`, `withKeyStore`, `withTree`, `sealShareAsync`, `unsealShareAsync`, `sealAccountPassphraseAsync`, `openAccountPassphraseAsync` |
| `KeyPairBytes` | `entry(power)`, `treeValidator(power)` |
| `Chain` | `produceBlock(timestamp, justify:)` |
| `TreeNode` | `withChild`, `find`, `chainIds` |
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
- Chain trees: `TreeNode`, `buildChainTree`, `ChainTree`.
- Custody: `KeyStore`, portable accounts, threshold accounts, `Ceremony`.
- Assurance and sealed mempools; anonymous membership credentials.

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

## Determinism

Every canonical byte string (hashes, transaction ids, headers, proofs) is
identical across the Go, npm, JVM, Swift and Dart bindings.
