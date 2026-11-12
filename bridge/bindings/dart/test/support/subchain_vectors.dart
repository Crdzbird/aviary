import 'dart:io';

/// The pinned values of vectors/subchain_v2_vectors.txt by name, or null when
/// the suite runs outside the repository and the file is not there.
Map<String, String>? loadSubchainVectors() {
  final file = File('../../../vectors/subchain_v2_vectors.txt');
  if (!file.existsSync()) return null;
  final out = <String, String>{};
  for (final raw in file.readAsLinesSync()) {
    final line = raw.trim();
    if (line.isEmpty || line.startsWith('#')) continue;
    final fields = line.split(RegExp(r'\s+'));
    out[fields[0]] = fields[1];
  }
  return out;
}
