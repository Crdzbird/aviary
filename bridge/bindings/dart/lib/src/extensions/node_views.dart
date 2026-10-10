part of '../../blockchainer.dart';

/// Decoded views of a node's state.
extension NodeViews on Node {
  /// Chain events since the last drain, decoded from JSON.
  List<Map<String, Object?>> events() {
    final decoded = jsonDecode(drainEvents());
    if (decoded is! List) return const [];
    return [
      for (final e in decoded)
        if (e is Map<String, Object?>) e,
    ];
  }

  /// Whether [proof] from [NodeQueries.proveAccount] holds under the
  /// current state root.
  bool verifyAccountProof(Uint8List proof) =>
      _bc.verifyStateProof(stateRoot(), proof);

  /// Prometheus metric values keyed by sample name with its labels verbatim.
  Map<String, double> metricValues() {
    final out = <String, double>{};
    for (final line in metrics().split('\n')) {
      if (line.isEmpty || line.startsWith('#')) continue;
      final space = line.lastIndexOf(' ');
      if (space < 0) continue;
      final value = double.tryParse(line.substring(space + 1));
      if (value != null) out[line.substring(0, space)] = value;
    }
    return out;
  }
}
