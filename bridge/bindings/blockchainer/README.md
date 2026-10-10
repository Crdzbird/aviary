# blockchainer

Blockchainer for Flutter: a C++ ledger with BFT finality, composable chains,
subchains, validator rotation, light clients and chain trees, driven over
dart:ffi. This package shares the CipherBird crypto engine with the
`cipherbird` package, which it depends on and re-exports, so an app that
uses both carries one engine.

Prefer `blockchainer_lite` when the app does not use CipherBird directly: it
bundles the engine and has no extra dependency.

## Install

```yaml
dependencies:
  blockchainer: ^0.1.0
```

Android, iOS and macOS are supported. The native library is bundled per
platform and loaded automatically; call `Blockchainer.ensureInitialized()`
once at start to warm it off the UI isolate.

## Use

```dart
import 'package:blockchainer/blockchainer.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Blockchainer.ensureInitialized();
  final bc = Blockchainer.instance;
  final node = bc.createNode(7);
  final alice = node.newAccount();
  node.credit(alice.address, 1000);
  node.produceBlock(1);
  node.dispose();
}
```

See the `blockchainer_dart` package for the full API: nodes, mixtures,
foreign components, subchains, rotation, light clients, chain trees,
custody, assurance, sealed mempools and membership credentials.
