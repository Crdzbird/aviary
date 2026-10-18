part of '../../aviary.dart';

/// A declared mixture of engine, state model and proposer ids plus params.
/// Commit it with [AviaryGenesis.commitBlueprint].
/// {@category Chains}
final class Blueprint {
  Blueprint._(this._bc, this.encoded);

  final Aviary _bc;

  /// The canonical bytes.
  final Uint8List encoded;

  /// The 32 byte commitment to the mixture.
  Uint8List hash() => _scoped(
    [encoded],
    (p) => _bc._take(
      _bc.lib
          .fn<_BufOfBytesC>('aviary_blueprint_hash')
          .asFunction<_BufOfBytesD>()(p[0], encoded.length),
      'blueprint_hash',
    ),
  );
}
