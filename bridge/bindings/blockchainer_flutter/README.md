# blockchainer_flutter

Blockchainer for Flutter — a full distributed-ledger stack via `dart:ffi`:
ledger + BFT consensus, mempool, observability, light-client verification, and
EVM / Bitcoin / Solana interop. The native library is bundled per platform and
loaded automatically; no setup, no library paths.

This is an **`ffiPlugin`** (no method channel): calls go straight to native, so
the ledger API is **fully synchronous** — once initialized, no `await` on
`createNode`, `submitTransfer`, `produceBlock`, etc.

## Install

```yaml
dependencies:
  blockchainer_flutter:
    # Until published, depend via path/git. blockchainer_flutter depends on
    # cryptolib_flutter, so check out the `cryptolib` repo next to `blockchainer`.
    path: ../blockchainer/bridge/bindings/blockchainer_flutter
```

> The crypto backend comes from the `cryptolib_flutter` package (depended on,
> not bundled). Both repos must be available side by side until `cryptolib` is
> published to pub.dev.

## Initialize off the UI isolate — no `await` required

Loading the crypto backend is a one-time cost. The heavy part — *mapping* the
native libraries — already happens at app launch, **off the UI thread**: on
Android the `BlockchainerStartup` `ContentProvider` maps the `.so` on a
background thread before Dart runs; on iOS/macOS `dyld` loads the embedded
framework at launch. So `Blockchainer.instance` is cheap on first use even with
no setup at all.

To also push the remaining `cryptolib_init` onto a background isolate **without
awaiting anything**, fire-and-forget `preload()`:

```dart
import 'package:blockchainer_flutter/blockchainer_flutter.dart';

void main() {
  Blockchainer.preload();          // isolate warm-up — NOT awaited
  runApp(const MyApp());
}

// ...anywhere, fully synchronous, no await:
final node = Blockchainer.instance.createNode(1);
```

That's the recommended pattern: **isolate-backed loading, no `await`, no UI
stall.** `instance` works immediately; `preload()` just makes the very first
call's init free too.

If instead you want to *gate* the UI on readiness (e.g. show a splash until the
backend is up), `await` the same warm-up:

```dart
Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Blockchainer.ensureInitialized(); // background load; completes when ready
  runApp(const MyApp());
}
// or, reactively:
FutureBuilder<void>(
  future: Blockchainer.ensureInitialized(),
  builder: (c, s) => s.connectionState == ConnectionState.done
      ? const HomeScreen() : const Center(child: CircularProgressIndicator()),
);
```

| API | `await`? | Use |
|---|---|---|
| `Blockchainer.preload()` | no | **Default.** Fire-and-forget isolate warm-up; use `instance` synchronously right after. |
| `Blockchainer.instance` | no | Lazy-synchronous singleton. Works even with no `preload()` (libs are mapped at launch). |
| `Blockchainer.ensureInitialized()` | yes | Only when you want to gate the UI on readiness. Idempotent + concurrency-safe. |
| `Blockchainer.isInitialized` | — | `true` once `instance` is ready with no load work pending. |

## Use

```dart
final bc = Blockchainer.instance;             // ready & synchronous
final node = bc.createNode(1);                 // a chain (id 1)

final alice = node.newAccount();
final bob = node.newAccount();
node.credit(alice.address, 1000);              // genesis credit
node.submitTransfer(alice.secret, bob.address, 250, node.nonce(alice.address));
node.produceBlock(DateTime.now().millisecondsSinceEpoch); // BFT finalize

node.balance(bob.address);                     // 250

// Light-client verification
bc.verifyStateProof(node.stateRoot(), node.proveAccount(bob.address)); // true
bc.verifyTxInclusion(txRoot, txProof);
bc.verifyCommit(chainId, validatorSet, commitCert);

// Streaming + observability
node.drainEvents();                            // JSON event feed
node.metrics();                                // Prometheus snapshot

// Interop addresses
bc.ethAddress(secret32);                       // 0x… (EIP-55)
bc.btcAddress(secret32);                       // bc1…
bc.solAddress(edSecret64);                     // base58

// EVM smart contracts (sign locally — no server needed)
bc.ethSelector('transfer(address,uint256)');           // 0xa9059cbb
final data = bc.ethErc20TransferCalldata(to20, amount);
final rawTx = bc.ethSignLegacy(nonce: 0, gasPrice: 30e9.toInt(), gasLimit: 60000,
    to: token20, data: data, chainId: 1, secret32: secp256k1Secret);
```

Free a node when done: `node.dispose()`.

## Platforms

| Platform | Native packaging | Warm-up |
|---|---|---|
| iOS / macOS | dynamic `BlockchainerC.xcframework` via Swift Package Manager (Flutter 3.44+) | `ensureInitialized()` (`Isolate`) |
| Android | per-ABI `jniLibs` (`libblockchainer_c.so`) | `BlockchainerStartup` `ContentProvider` (auto, off-thread) + `ensureInitialized()` |

`blockchainer_c`'s `cryptolib_*` symbols are left undefined and resolve at
runtime against the `CryptoLibC` framework / `libcryptolib_c.so` that
`cryptolib_flutter` embeds.

## Testing on the host

The pure-Dart logic can be unit-tested on the desktop VM by pointing at a
locally-built dylib:

```bash
BLOCKCHAINER_DYLIB=/path/to/build/libblockchainer_c.dylib flutter test
```

See `example/` for a minimal app and `examples/blockchainer_demo` in the repo for
a full dashboard.
