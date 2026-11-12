part of '../../aviary.dart';

/// Registry operations: the unsigned bodies a registrar then signs.
/// {@category Subchains and trees}
extension AviarySubchainOps on Aviary {
  /// An unsigned operation registering a child from its encoded
  /// registration, valid in [authorityEpoch] up to parent height
  /// [validUntil], where 0 means no expiry.
  Uint8List opRegister(
    Uint8List registration, {
    int authorityEpoch = 0,
    int validUntil = 0,
  }) => _scoped(
    [registration],
    (p) => _take(
      lib
          .fn<_OpRegisterC>('aviary_subchain_op_register')
          .asFunction<_OpRegisterD>()(
        authorityEpoch,
        validUntil,
        p[0],
        registration.length,
      ),
      'subchain_op_register',
    ),
  );

  /// An unsigned operation closing [chainId] at its recorded tip, as a
  /// [ChildRecord] reports it.
  Uint8List opClose({
    required int chainId,
    required int finalHeight,
    required Uint8List finalTipHash,
    int authorityEpoch = 0,
    int validUntil = 0,
  }) {
    if (finalTipHash.length != 32) {
      throw const AviaryException('final tip hash must be 32 bytes');
    }
    return _scoped(
      [finalTipHash],
      (p) => _take(
        lib.fn<_OpCloseC>('aviary_subchain_op_close').asFunction<_OpCloseD>()(
          authorityEpoch,
          validUntil,
          chainId,
          finalHeight,
          p[0],
        ),
        'subchain_op_close',
      ),
    );
  }

  /// An unsigned operation replacing the registrar set. Applying it grows
  /// the authority epoch, which voids every operation signed for the old one.
  Uint8List opSetRegistrars({
    required int threshold,
    required List<RegistrarEntry> registrars,
    int authorityEpoch = 0,
    int validUntil = 0,
  }) {
    final keys = _registrarList(registrars);
    return _scoped(
      [keys],
      (p) => _take(
        lib
            .fn<_OpRegistrarsC>('aviary_subchain_op_set_registrars')
            .asFunction<_OpRegistrarsD>()(
          authorityEpoch,
          validUntil,
          threshold,
          p[0],
          keys.length,
        ),
        'subchain_op_set_registrars',
      ),
    );
  }
}
