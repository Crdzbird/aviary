part of '../../aviary.dart';

/// Light clients that follow a header chain with certificates only.
extension AviaryLightClients on Aviary {
  /// A light client trusting [trustedHeader] and the set in force for the
  /// next header. Null when the input is malformed.
  LightClient? newLightClient(
    int chainId,
    List<ValidatorEntry> validators,
    Uint8List trustedHeader,
  ) {
    final vs = _encodeValidatorSet(validators);
    return _scoped([vs, trustedHeader], (p) {
      final h = lib.lookupFunction<_LcNewC, _LcNewD>('aviary_light_client_new')(
          chainId, p[0], vs.length, p[1], trustedHeader.length);
      if (h == nullptr) return null;
      return LightClient._(this, h);
    });
  }
}

/// Bisection sync for a light client.
extension LightClientSync on LightClient {
  /// Reaches [target] at [targetHeight], fetching intermediate checkpoints
  /// while the overlap is too thin. Throws on a hard refusal or a missing
  /// checkpoint.
  void syncTo(
    Checkpoint target,
    int targetHeight,
    Checkpoint? Function(int height) fetch,
  ) {
    final stack = <(Checkpoint, int)>[(target, targetHeight)];
    for (var n = 0; stack.isNotEmpty; n++) {
      if (n > 128) throw StateError('light client: fetch budget exceeded');
      final (cp, hi) = stack.last;
      final r = acceptSkipping(cp);
      if (r == SkipResult.accepted) {
        stack.removeLast();
        continue;
      }
      if (r != SkipResult.insufficientOverlap) {
        throw StateError('light client: checkpoint refused');
      }
      final lo = height;
      if (hi - lo <= 1) {
        throw StateError('light client: no intermediate header to bisect to');
      }
      final mid = lo + (hi - lo) ~/ 2;
      final m = fetch(mid);
      if (m == null) throw StateError('light client: no checkpoint at $mid');
      stack.add((m, mid));
    }
  }
}
