# blockchainer_dart

The Dart API of Blockchainer: a C++ ledger with BFT finality, composable
chains, subchains, validator rotation, light clients and chain trees, driven
over dart:ffi. Cryptography comes from the CipherBird engine.

Flutter apps should depend on `blockchainer` (shares the engine with the
`cipherbird` package) or `blockchainer_lite` (self contained) instead; both
re-export this API.

## Install

```yaml
dependencies:
  blockchainer_dart: ^0.1.0
```

The engine for macOS arm64 ships inside the package. On other hosts point
`BLOCKCHAINER_DYLIB` at a built `libblockchainer_c` library.

## Use

```dart
import 'package:blockchainer_dart/blockchainer.dart';

void main() {
  final bc = Blockchainer.instance;
  final node = bc.createNode(7);
  final alice = node.newAccount();
  final bob = node.newAccount();
  node.credit(alice.address, 1000);
  node.submitTransfer(alice.secret, bob.address, 250, node.nonce(alice.address));
  node.produceBlock(1);
  final proof = node.proveAccount(bob.address);
  print(bc.verifyStateProof(node.stateRoot(), proof));
  node.dispose();
}
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

## Determinism

Every canonical byte string (hashes, transaction ids, headers, proofs) is
identical across the Go, npm, JVM, Swift and Dart bindings.
