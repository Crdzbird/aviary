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
  aviary: ^1.0.0
```

Android, iOS and macOS are supported. Linux desktop is not, because the
`cipherbird` package this one shares its engine with has no Linux build; use
`aviary_lite` there. The native library is bundled per platform and loaded
automatically; call `Aviary.ensureInitialized()`
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

## Example

The example app has four tabs: a ledger round trip with a balance proof, a
live event stream with metrics, an equipment record that is registered,
serviced, handed over and proven, and four validators reaching consensus
over a `PeerMesh`. Run it with `flutter run` from `example/`.

## How the engine is shared

On iOS and macOS the app embeds both `AviaryC.framework` and
`CipherBird.framework`; Aviary's `cryptolib_*` symbols resolve at load time
against CipherBird. On Android the plugin loads `libcipherbird.so` before
`libaviary_c.so`, because the CipherBird library's SONAME is still
`libcryptolib_c.so` and the dynamic linker would not find it by its file
name otherwise. Both loads happen in the plugin's startup provider and in
the Dart loader, so an app does not have to do anything.
