part of '../../blockchainer.dart';

/// A declared mixture of engine, state model and proposer ids plus params.
/// Commit it with [BlockchainerGenesis.commitBlueprint].
final class Blueprint {
  Blueprint._(this._bc, this.encoded);

  final Blockchainer _bc;

  /// The canonical bytes.
  final Uint8List encoded;

  /// The 32 byte commitment to the mixture.
  Uint8List hash() => _scoped(
        [encoded],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
              'blockchainer_blueprint_hash')(p[0], encoded.length),
          'blueprint_hash',
        ),
      );
}
