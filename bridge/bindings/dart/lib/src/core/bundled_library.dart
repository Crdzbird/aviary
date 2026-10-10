part of '../../aviary.dart';

const _bundledPackages = ['aviary_dart', 'aviary', 'aviary_lite'];

/// The engine shipped inside the package under the native directory, if any.
String? _bundledLibraryPath() {
  final slot = _nativeSlot();
  if (slot == null) return null;
  final roots = <String>[
    ..._packageRootsFromConfig(),
    Directory.current.path,
    if (Platform.script.scheme == 'file')
      File.fromUri(Platform.script).parent.parent.path,
  ];
  for (final root in roots) {
    final candidate = '$root/native/$slot/${_libraryFileName()}';
    if (File(candidate).existsSync()) return candidate;
  }
  return null;
}

String? _nativeSlot() {
  final os = _osSlot();
  if (os == null) return null;
  final abi = Abi.current().toString();
  if (abi.endsWith('arm64')) return '$os-arm64';
  if (abi.endsWith('x64')) return '$os-x64';
  return null;
}

String? _osSlot() {
  if (Platform.isMacOS) return 'darwin';
  if (Platform.isLinux) return 'linux';
  if (Platform.isWindows) return 'windows';
  return null;
}

String _libraryFileName() {
  if (Platform.isMacOS) return 'libaviary_c.dylib';
  if (Platform.isWindows) return 'aviary_c.dll';
  return 'libaviary_c.so';
}

List<String> _packageRootsFromConfig() {
  final config =
      File('${Directory.current.path}/.dart_tool/package_config.json');
  if (!config.existsSync()) return const [];
  final decoded = jsonDecode(config.readAsStringSync()) as Map<String, dynamic>;
  final packages = decoded['packages'];
  if (packages is! List) return const [];
  return [
    for (final entry in packages.whereType<Map<String, dynamic>>())
      if (_bundledPackages.contains(entry['name']))
        config.parent.uri.resolve(entry['rootUri'] as String).toFilePath(),
  ];
}
