part of '../aviary_web.dart';

/// The loaded WebAssembly engine and the stateless entry points. The
/// engine loads asynchronously: await [ensureInitialized] once, then use
/// [instance], [load] or [open] as on other platforms.
/// {@category Ledger}
final class Aviary {
  Aviary._(this._engine);

  final _Engine _engine;

  static Aviary? _singleton;
  static Future<_Engine>? _loading;

  /// The loaded engine. On the web [path] is not used: the engine comes from
  /// the URL [ensureInitialized] loaded it from.
  static Aviary open(String path) => instance;

  /// The loaded engine; [path] is not used on the web.
  static Aviary load([String? path]) => instance;

  /// The engine, once [ensureInitialized] completed. Throws an
  /// [AviaryException] before that, because a browser cannot load
  /// WebAssembly synchronously.
  static Aviary get instance =>
      _singleton ??
      (throw const AviaryException(
        'the web engine loads asynchronously: '
        'await Aviary.ensureInitialized() first',
      ));

  /// `web` once the engine is loaded, null before.
  static String? get loadedFrom => _singleton == null ? null : 'web';

  /// Whether [instance] is ready.
  static bool get isInitialized => _singleton != null;

  /// Alias of [isInitialized].
  static bool get initialized => isInitialized;

  /// Fetches and instantiates the engine once. The module URL is
  /// `self.aviaryEngineUrl` when the page sets it, else the
  /// `AVIARY_WEB_ENGINE` compile time define, else the copy Flutter serves
  /// from this package's assets; a relative URL resolves against the
  /// document's base URI. A failed load is retried on the next call.
  static Future<void> ensureInitialized() async {
    if (_singleton != null) return;
    final loading = _loading ??= _loadEngine(_engineUrl());
    try {
      _singleton ??= Aviary._(await loading);
    } on Object {
      _loading = null;
      rethrow;
    }
  }

  /// Alias of [ensureInitialized].
  static Future<void> init() => ensureInitialized();

  /// Loads the engine. Returns false when the load failed.
  static Future<bool> preload() async {
    try {
      await ensureInitialized();
      return true;
    } on Object catch (_) {
      return false;
    }
  }

  /// The library version string.
  String version() => _engine.cString('aviary_version');

  /// The crypto backend version string, with the CipherBird revision the
  /// engine was built from.
  String cryptoBackend() => _engine.cString('aviary_crypto_backend_version');

  /// Alias of [cryptoBackend].
  String cryptoBackendVersion() => cryptoBackend();
}
