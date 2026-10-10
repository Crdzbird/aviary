part of '../../blockchainer.dart';

/// A mempool of sealed transactions only the block proposer can open.
final class SealedMempool {
  SealedMempool._(this._bc, this._h);

  final Blockchainer _bc;
  final Pointer<Void> _h;

  /// Admits a sealed transaction. False when the pool is at capacity.
  bool add(Uint8List sealedTx) => _scoped(
        [sealedTx],
        (p) =>
            _bc.lib.lookupFunction<_SmAddC, _SmAddD>(
                'blockchainer_sealed_mempool_add')(_h, p[0], sealedTx.length) ==
            1,
      );

  /// Sealed transactions currently held.
  int get size => _bc.lib.lookupFunction<_SmSizeC, _SmSizeD>(
      'blockchainer_sealed_mempool_size')(_h);

  /// Reveals up to [max] transactions with the proposer's KEM secret. Any
  /// other key opens nothing.
  List<Uint8List> reveal(Uint8List proposerKemSecret, int max) => _scoped(
        [proposerKemSecret],
        (p) => _splitBlobList(_bc._take(
          _bc.lib.lookupFunction<_SmRevealC, _SmRevealD>(
                  'blockchainer_sealed_mempool_reveal')(
              _h, p[0], proposerKemSecret.length, max),
          'sealed_mempool_reveal',
        )),
      );

  /// Releases the native pool.
  void free() => _bc.lib.lookupFunction<_HandleFreeC, _HandleFreeD>(
      'blockchainer_sealed_mempool_free')(_h);
}
