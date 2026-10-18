part of '../../aviary.dart';

@Native<Size Function(Pointer<Utf8>, Size)>(symbol: 'aviary_library_path')
external int _aviaryLibraryPath(Pointer<Utf8> out, int cap);

/// Where the engine was found: explicit, asset, process, bundled or system.
String? _loadedFrom;

DynamicLibrary _found(String source, DynamicLibrary lib) {
  _loadedFrom = source;
  return lib;
}

DynamicLibrary _openPlatformLibrary(String? path) {
  final explicit = path ?? Platform.environment['AVIARY_DYLIB'];
  if (explicit != null && explicit.isNotEmpty) {
    return _found('explicit', DynamicLibrary.open(explicit));
  }
  final asset = _openBundledAsset();
  if (asset != null) return _found('asset', asset);
  if (Platform.isAndroid) return _found('system', _openAndroid());
  final process = DynamicLibrary.process();
  if (process.providesSymbol('aviary_init')) return _found('process', process);
  final bundled = _bundledLibraryPath();
  if (bundled != null) return _found('bundled', DynamicLibrary.open(bundled));
  if (Platform.isWindows) {
    return _found('system', DynamicLibrary.open('aviary_c.dll'));
  }
  if (Platform.isLinux) {
    return _found('system', DynamicLibrary.open('libaviary_c.so'));
  }
  return _found('process', process);
}

/// The library the build hook bundled as a code asset, reopened by its own
/// path so every symbol can be looked up. Null when no asset was bundled.
DynamicLibrary? _openBundledAsset() {
  final out = calloc<Uint8>(4096);
  try {
    final n = _aviaryLibraryPath(out.cast(), 4096);
    if (n <= 0) return null;
    return DynamicLibrary.open(utf8.decode(out.asTypedList(n)));
  } on Object catch (_) {
    return null;
  } finally {
    calloc.free(out);
  }
}

DynamicLibrary _openAndroid() {
  try {
    DynamicLibrary.open('libcipherbird.so');
  } on ArgumentError catch (_) {
    return DynamicLibrary.open('libaviary_c.so');
  }
  return DynamicLibrary.open('libaviary_c.so');
}

final _symbols = Expando<Map<String, Pointer<Void>>>();

/// Native symbols resolved once per library. A symbol lookup costs far more
/// than the call it serves, so each name is looked up on first use only.
extension on DynamicLibrary {
  Pointer<NativeFunction<T>> fn<T extends Function>(String name) {
    final cache = _symbols[this] ??= <String, Pointer<Void>>{};
    return (cache[name] ??= lookup<Void>(name)).cast<NativeFunction<T>>();
  }
}
