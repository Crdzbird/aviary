/// Blockchainer for Flutter — public API.
///
/// Loading the native library + initializing the crypto backend is a one-time
/// cost that can take tens of milliseconds, so do it **off the UI isolate**:
///
/// ```dart
/// Future<void> main() async {
///   WidgetsFlutterBinding.ensureInitialized();
///   await Blockchainer.ensureInitialized(); // background load — no UI stall
///   runApp(const MyApp());
/// }
///
/// // ...anywhere after that, fully synchronous — no await on ledger calls:
/// final node = Blockchainer.instance.createNode(1);
/// ```
///
/// `Blockchainer.ensureInitialized()` warms the libraries on a background
/// isolate and completes when [Blockchainer.instance] is ready (it is idempotent
/// and concurrency-safe — also fine inside a `FutureBuilder`). If you skip it,
/// `Blockchainer.instance` still works: it loads lazily on first use, just on
/// the calling isolate. `Blockchainer.preload()` is the fire-and-forget variant.
///
/// The crypto backend is provided by the `cryptolib_flutter` package, whose
/// native `CryptoLibC` (iOS/macOS) / `libcryptolib_c.so` (Android) is embedded
/// in the process; Blockchainer's native links it at runtime.
library;

export 'package:blockchainer_dart/blockchainer.dart';

import 'package:blockchainer_dart/blockchainer.dart';

/// Convenience for the lazy-synchronous singleton, equivalent to
/// [Blockchainer.instance].
///
/// Prefer `await Blockchainer.ensureInitialized()` once at app start (so the
/// load happens off the UI isolate), then use `Blockchainer.instance` directly.
@Deprecated('Use Blockchainer.ensureInitialized() + Blockchainer.instance')
Blockchainer loadBlockchainer() => Blockchainer.instance;
