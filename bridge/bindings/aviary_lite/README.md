# aviary_lite

Aviary for Flutter, self contained: a C++ ledger with BFT finality,
composable chains, subchains, validator rotation, light clients and chain
trees, driven over dart:ffi. The CipherBird crypto engine is merged into the
native library, so there is no other dependency to add.

Prefer `aviary` when the app also uses the `cipherbird` package: that
variant shares one engine with it instead of carrying a second copy.

## Install

```yaml
dependencies:
  aviary_lite: ^0.1.0
```

Android, iOS, macOS and Linux desktop are supported. The native library is
bundled per platform and loaded automatically (on Linux through
`aviary_dart`'s build hook, so a Linux app needs no setup); call `Aviary.ensureInitialized()`
once at start to warm it off the UI isolate.

## Use

```dart
import 'package:aviary_lite/aviary_lite.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await AviaryLite.ensureInitialized();
  runApp(const MyApp());
}
```

Then anywhere in the app:

```dart
final node = AviaryLite.instance.createNode(7);
final alice = node.fundAccount(1000);
final bob = node.fundAccount(0);
node.sendTo(alice, bob, 250);
node.commit();
final ok = node.verifyAccountProof(node.proveAccount(bob.address));
```

`AviaryLite` is a thin front for `Aviary`; the whole `aviary_dart`
API is re-exported, including its shortcut extensions (`fundAccount`,
`sendTo`, `commit`, `hex`, `assembleSingleValidator`, `signWithShares`,
`produceRound`). See that package for the full surface.

## Example

The example app has four tabs: a ledger round trip with a balance proof, a
live event stream with metrics, an equipment record that is registered,
serviced, handed over and proven, and four validators reaching consensus
over a `PeerMesh`. Run it with `flutter run` from `example/`.
