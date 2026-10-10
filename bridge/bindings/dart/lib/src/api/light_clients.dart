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
