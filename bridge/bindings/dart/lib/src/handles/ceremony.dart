part of '../../aviary.dart';

/// A two round threshold signing ceremony over one message and roster.
/// {@category Custody}
final class Ceremony extends _NativeHandle {
  Ceremony._(Aviary bc, Pointer<Void> h)
    : super(bc, h, 'aviary_threshold_ceremony_free');

  /// Round two for [card] with its round one [nonces]: a signature share.
  Uint8List sign(Uint8List card, Uint8List nonces) => _scoped(
    [card, nonces],
    (p) => _bc._take(
      _bc.lib.fn<_ThSignC>('aviary_threshold_sign').asFunction<_ThSignD>()(
        _h,
        p[0],
        card.length,
        p[1],
      ),
      'threshold_sign',
    ),
  );

  /// Attributes a share to participant [id] so a lying cosigner is named.
  bool verifyShare(int id, Uint8List publicShare, Uint8List sigShare) =>
      _scoped(
        [publicShare, sigShare],
        (p) =>
            _bc.lib
                .fn<_ThVerifyC>('aviary_threshold_verify_share')
                .asFunction<_ThVerifyD>()(_h, id, p[0], p[1]) ==
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
          _bc.lib
              .fn<_ThAggC>('aviary_threshold_aggregate')
              .asFunction<_ThAggD>()(_h, idP, p[0], ids.length),
          'threshold_aggregate',
        ),
      );
    } finally {
      calloc.free(idP);
    }
  }

  /// Releases the native ceremony.
  void free() => _release();
}

ThresholdShare _shareInfo(Aviary bc, Uint8List card) => _scoped([card], (p) {
  final info = bc._take(
    bc.lib
        .fn<_BufOfBytesC>('aviary_threshold_share_info')
        .asFunction<_BufOfBytesD>()(p[0], card.length),
    'threshold_share_info',
  );
  return ThresholdShare(
    card,
    (info[0] << 8) | info[1],
    Uint8List.sublistView(info, 2, 34),
    Uint8List.sublistView(info, 34, 66),
  );
});
