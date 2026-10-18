part of '../../aviary.dart';

/// A light client that follows a header chain with certificates only.
/// Sequential [accept], plus the overlap rule in [acceptSkipping]: a far
/// header is trusted when its own set certifies it and the signers hold
/// more than the trust level of the set already trusted.
/// {@category Light clients}
final class LightClient extends _NativeHandle {
  LightClient._(Aviary bc, Pointer<Void> h)
    : super(bc, h, 'aviary_light_client_free');

  /// The trusted height.
  int get height => _bc.lib
      .fn<_U64OfHandleC>('aviary_light_client_height')
      .asFunction<_U64OfHandleD>()(_h);

  Uint8List _buf(String fn) => _bc._take(
    _bc.lib.fn<_BufOfHandleC>(fn).asFunction<_BufOfHandleD>()(_h),
    fn,
  );

  /// The trusted tip header.
  Uint8List tip() => _buf('aviary_light_client_tip');

  /// The set in force for the next header.
  List<ValidatorEntry> validators() =>
      _decodeValidatorSet(_buf('aviary_light_client_validators'));

  /// The overlap threshold as a fraction; bad values are ignored.
  void setTrustLevel(int numerator, int denominator) => _bc.lib
      .fn<_LcTrustC>('aviary_light_client_set_trust_level')
      .asFunction<_LcTrustD>()(_h, numerator, denominator);

  /// The largest single jump in blocks, 0 for unbounded.
  void setMaxSkip(int blocks) => _bc.lib
      .fn<_LcMaxSkipC>('aviary_light_client_set_max_skip')
      .asFunction<_LcMaxSkipD>()(_h, blocks);

  /// The sequential rule.
  bool accept(Uint8List header, Uint8List certificate) => _scoped(
    [header, certificate],
    (p) =>
        _bc.lib
            .fn<_LcAcceptC>('aviary_light_client_accept')
            .asFunction<_LcAcceptD>()(
          _h,
          p[0],
          header.length,
          p[1],
          certificate.length,
        ) ==
        1,
  );

  /// The overlap rule for a checkpoint.
  SkipResult acceptSkipping(Checkpoint cp) {
    final vs = _encodeValidatorSet(cp.validators);
    final r = _scoped(
      [cp.header, cp.certificate, vs],
      (p) =>
          _bc.lib
              .fn<_LcSkipC>('aviary_light_client_accept_skipping')
              .asFunction<_LcSkipD>()(
            _h,
            p[0],
            cp.header.length,
            p[1],
            cp.certificate.length,
            p[2],
            vs.length,
          ),
    );
    if (r == 1) return SkipResult.accepted;
    if (r == 2) return SkipResult.insufficientOverlap;
    return SkipResult.refused;
  }

  /// Releases the native client.
  void free() => _release();
}
