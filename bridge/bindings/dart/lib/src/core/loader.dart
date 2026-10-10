part of '../../blockchainer.dart';

DynamicLibrary _openPlatformLibrary(String? path) {
  final explicit = path ?? Platform.environment['BLOCKCHAINER_DYLIB'];
  if (explicit != null && explicit.isNotEmpty) {
    return DynamicLibrary.open(explicit);
  }
  if (Platform.isAndroid) return _openAndroid();
  final process = DynamicLibrary.process();
  if (process.providesSymbol('blockchainer_init')) return process;
  final bundled = _bundledLibraryPath();
  if (bundled != null) return DynamicLibrary.open(bundled);
  if (Platform.isWindows) return DynamicLibrary.open('blockchainer_c.dll');
  if (Platform.isLinux) return DynamicLibrary.open('libblockchainer_c.so');
  return process;
}

DynamicLibrary _openAndroid() {
  try {
    DynamicLibrary.open('libcipherbird.so');
  } on ArgumentError catch (_) {
    return DynamicLibrary.open('libblockchainer_c.so');
  }
  return DynamicLibrary.open('libblockchainer_c.so');
}
