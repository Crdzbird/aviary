part of '../../blockchainer.dart';

/// The loaded native library and the stateless entry points. Feature
/// surfaces are extensions on this class; stateful objects are created
/// through it.
final class Blockchainer {
  Blockchainer._(this.lib);

  /// The resolved native library.
  final DynamicLibrary lib;

  static Blockchainer? _singleton;
  static Future<bool>? _warming;

  /// Opens the native library at [path] and initializes it.
  static Blockchainer open(String path) =>
      fromLibrary(DynamicLibrary.open(path));

  /// Wraps an already resolved [lib] and runs the one time native init.
  static Blockchainer fromLibrary(DynamicLibrary lib) {
    final bc = Blockchainer._(lib);
    final rc =
        lib.lookupFunction<_IntVoid, int Function()>('blockchainer_init');
    if (rc() != 0) throw const BlockchainerException('init failed');
    return bc;
  }

  /// Opens the library the platform default way, or [path] if given, or the
  /// BLOCKCHAINER_DYLIB environment override.
  static Blockchainer load([String? path]) =>
      fromLibrary(_openPlatformLibrary(path));

  /// The process wide instance, loaded synchronously on first access.
  static Blockchainer get instance => _singleton ??= load();

  /// Whether [instance] is ready without any load work on this isolate.
  static bool get isInitialized => _singleton != null;

  /// Alias of [isInitialized].
  static bool get initialized => isInitialized;

  /// Warms the native library on a background isolate, then prepares
  /// [instance]. Idempotent and safe to call concurrently.
  static Future<void> ensureInitialized() async {
    if (_singleton != null) return;
    _warming ??= preload();
    await _warming;
    _singleton ??= load();
  }

  /// Alias of [ensureInitialized].
  static Future<void> init() => ensureInitialized();

  /// Maps the library and runs the crypto backend init on a background
  /// isolate. Returns false when the warm up failed; the lazy path still works.
  static Future<bool> preload() async {
    try {
      return await Isolate.run<bool>(() {
        load().version();
        return true;
      });
    } on Object catch (_) {
      return false;
    }
  }

  /// The library version string.
  String version() => _cString('blockchainer_version');

  /// The crypto backend version string.
  String cryptoBackend() => _cString('blockchainer_crypto_backend_version');

  /// Alias of [cryptoBackend].
  String cryptoBackendVersion() => cryptoBackend();

  String _cString(String symbol) =>
      lib.lookupFunction<_PtrVoid, _PtrVoid>(symbol)().toDartString();

  Uint8List _take(_BufResult r, String op) {
    if (r.buf.data == nullptr) throw _takeError(r, op);
    final out = Uint8List.fromList(r.buf.data.asTypedList(r.buf.len));
    final tmp = calloc<_Buffer>();
    tmp.ref.data = r.buf.data;
    tmp.ref.len = r.buf.len;
    lib.lookupFunction<_BufFreeC, _BufFreeD>('blockchainer_buffer_free')(tmp);
    calloc.free(tmp);
    return out;
  }

  BlockchainerException _takeError(_BufResult r, String op) {
    if (r.error == nullptr) return BlockchainerException('$op failed');
    final message = r.error.toDartString();
    lib.lookupFunction<_StrFreeC, _StrFreeD>('blockchainer_str_free')(r.error);
    return BlockchainerException(message);
  }

  String _takeStr(_BufResult r, String op) => utf8.decode(_take(r, op));
}
