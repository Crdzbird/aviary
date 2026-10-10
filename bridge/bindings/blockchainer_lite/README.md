# blockchainer_lite

Blockchainer for Flutter, self contained: a C++ ledger with BFT finality,
composable chains, subchains, validator rotation, light clients and chain
trees, driven over dart:ffi. The CipherBird crypto engine is merged into the
native library, so there is no other dependency to add.

Prefer `blockchainer` when the app also uses the `cipherbird` package: that
variant shares one engine with it instead of carrying a second copy.

## Install

```yaml
dependencies:
  blockchainer_lite: ^0.1.0
```

Android, iOS and macOS are supported. The native library is bundled per
platform and loaded automatically; call `Blockchainer.ensureInitialized()`
once at start to warm it off the UI isolate.

## Use

```dart
import 'package:blockchainer_lite/blockchainer_lite.dart';

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

See the `blockchainer_dart` package for the full API.
