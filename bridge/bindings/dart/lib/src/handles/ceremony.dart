part of '../../aviary.dart';

/// A two round threshold signing ceremony over one message and roster.
final class Ceremony {
  Ceremony._(this._bc, this._h);

  final Aviary _bc;
  final Pointer<Void> _h;

  /// Round two for [card] with its round one [nonces]: a signature share.
  Uint8List sign(Uint8List card, Uint8List nonces) => _scoped(
        [card, nonces],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_ThSignC, _ThSignD>('aviary_threshold_sign')(
              _h, p[0], card.length, p[1]),
          'threshold_sign',
        ),
      );

  /// Attributes a share to participant [id] so a lying cosigner is named.
  bool verifyShare(int id, Uint8List publicShare, Uint8List sigShare) =>
      _scoped(
        [publicShare, sigShare],
        (p) =>
            _bc.lib.lookupFunction<_ThVerifyC, _ThVerifyD>(
                'aviary_threshold_verify_share')(_h, id, p[0], p[1]) ==
            1,
      );

  /// Aggregates id and share pairs into a standard Ed25519 signature.
  Uint8List aggregate(List<int> ids, List<Uint8List> sigShares) {
    final idP = calloc<Uint16>(ids.isEmpty ? 1 : ids.length);
    for (var i = 0; i < ids.length; i++) {
      idP[i] = ids[i];
    }
    try {
      return _scoped(
        [_concat(sigShares)],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_ThAggC, _ThAggD>(
              'aviary_threshold_aggregate')(_h, idP, p[0], ids.length),
          'threshold_aggregate',
        ),
      );
    } finally {
      calloc.free(idP);
    }
  }

  /// Releases the native ceremony.
  void free() => _bc.lib.lookupFunction<_HandleFreeC, _HandleFreeD>(
      'aviary_threshold_ceremony_free')(_h);
}
