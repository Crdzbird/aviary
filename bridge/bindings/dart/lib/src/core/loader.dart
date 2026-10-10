part of '../../aviary.dart';

DynamicLibrary _openPlatformLibrary(String? path) {
  final explicit = path ?? Platform.environment['AVIARY_DYLIB'];
  if (explicit != null && explicit.isNotEmpty) {
    return DynamicLibrary.open(explicit);
  }
  if (Platform.isAndroid) return _openAndroid();
  final process = DynamicLibrary.process();
  if (process.providesSymbol('aviary_init')) return process;
  final bundled = _bundledLibraryPath();
  if (bundled != null) return DynamicLibrary.open(bundled);
  if (Platform.isWindows) return DynamicLibrary.open('aviary_c.dll');
  if (Platform.isLinux) return DynamicLibrary.open('libaviary_c.so');
  return process;
}

DynamicLibrary _openAndroid() {
  try {
    DynamicLibrary.open('libcipherbird.so');
  } on ArgumentError catch (_) {
    return DynamicLibrary.open('libaviary_c.so');
  }
  return DynamicLibrary.open('libaviary_c.so');
}
