import 'dart:io';

import 'package:code_assets/code_assets.dart';
import 'package:hooks/hooks.dart';

/// Bundles the prebuilt engine for Linux and Windows targets from
/// the native directory for the target as a code asset, so a Dart or Flutter application on
/// those platforms gets the library next to its executable. Apple and
/// Android targets are served by the aviary and aviary_lite plugins, and the
/// macOS pure Dart case by the package root lookup, so the hook stays quiet
/// there. A target without a prebuilt binary also stays quiet; the loader then
/// falls back to AVIARY_DYLIB or a system wide library.
void main(List<String> args) async {
  await build(args, (input, output) async {
    if (!input.config.buildCodeAssets) return;
    final os = input.config.code.targetOS;
    if (os != OS.linux && os != OS.windows) return;
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

String? _slot(OS os, Architecture arch) {
  if (arch != Architecture.x64 && arch != Architecture.arm64) return null;
  return '${os.name}-${arch.name}';
}
