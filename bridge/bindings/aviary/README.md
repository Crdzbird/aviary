# aviary

Aviary for Flutter: a C++ ledger with BFT finality, composable chains,
subchains, validator rotation, light clients and chain trees, driven over
dart:ffi. This package shares the CipherBird crypto engine with the
`cipherbird` package, which it depends on and re-exports, so an app that
uses both carries one engine.

Prefer `aviary_lite` when the app does not use CipherBird directly: it
bundles the engine and has no extra dependency.

## Install

```yaml
dependencies:
  aviary: ^0.1.0
```

Android, iOS and macOS are supported. The native library is bundled per
platform and loaded automatically; call `Aviary.ensureInitialized()`
once at start to warm it off the UI isolate.

## Use

```dart
import 'package:aviary/aviary.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await AviaryShared.ensureInitialized();
  runApp(const MyApp());
}
```

Then anywhere in the app:

```dart
final node = AviaryShared.instance.createNode(7);
final alice = node.fundAccount(1000);
final bob = node.fundAccount(0);
node.sendTo(alice, bob, 250);
node.commit();
final ok = node.verifyAccountProof(node.proveAccount(bob.address));
```

`AviaryShared` is a thin front for `Aviary`; the whole `aviary_dart`
API is re-exported, including its shortcut extensions (`fundAccount`,
`sendTo`, `commit`, `hex`, `assembleSingleValidator`, `signWithShares`,
`produceRound`). See that package for the full surface.
