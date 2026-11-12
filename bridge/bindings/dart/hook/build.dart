import 'dart:io';
import 'dart:isolate';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

/// The Flutter plugins that link the macOS engine into the app through their
/// own xcframeworks.
const _macOSPlugins = ['aviary', 'aviary_lite'];

/// Bundles the prebuilt engine from the native directory for the target as a
/// code asset, so a Dart or Flutter application on Linux, Windows or macOS
/// carries the library with its executable and loads it with no package
/// config and no AVIARY_DYLIB.
///
/// On macOS the hook stays quiet when the application depends on the aviary
/// or aviary_lite plugin: those link the engine themselves (aviary against
/// the CipherBird framework of the cipherbird package), and a second copy
/// would end up in the app. The hook runs with the package config of the
/// application, so that check sees the whole dependency graph. iOS and
/// Android are always served by the plugins. A target without a prebuilt
/// binary also stays quiet; the loader then falls back to AVIARY_DYLIB or a
/// system wide library.
void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final os = input.config.code.targetOS;
    if (!_bundles(os, output)) return;
    final slot = _slot(os, input.config.code.targetArchitecture);
    if (slot == null) return;
    final file = input.packageRoot.resolve(
      'native/$slot/${os.libraryFileName('aviary_c', DynamicLoadingBundled())}',
    );
    if (!File.fromUri(file).existsSync()) return;
    output.assets.code.add(
      CodeAsset(
        package: input.packageName,
        name: 'aviary.dart',
        linkMode: DynamicLoadingBundled(),
        file: file,
      ),
    );
  });
}

bool _bundles(OS os, BuildOutputBuilder output) {
  if (os == OS.linux || os == OS.windows) return true;
  if (os != OS.macOS) return false;
  final config = Isolate.packageConfigSync;
  if (config != null) output.dependencies.add(config);
  return !_macOSPlugins.any(_inGraph);
}

bool _inGraph(String package) =>
    Isolate.resolvePackageUriSync(Uri.parse('package:$package/')) != null;

String? _slot(OS os, Architecture arch) {
  if (arch != Architecture.x64 && arch != Architecture.arm64) return null;
  if (os == OS.macOS) return 'darwin-${arch.name}';
  return '${os.name}-${arch.name}';
}
