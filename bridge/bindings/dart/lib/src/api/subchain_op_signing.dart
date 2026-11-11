part of '../../aviary.dart';

/// Authorising registry operations as a registrar.
/// {@category Subchains and trees}
extension AviarySubchainOpSigning on Aviary {
  void _requireHash(Uint8List hash, String what) {
    if (hash.length != 32) throw AviaryException('$what must be 32 bytes');
  }

  /// The 32 byte digest registrars sign for [op] on the parent identified by
  /// its chain id and genesis hash, for a signer outside this library.
  Uint8List opDigest(
    Uint8List op,
    int parentChainId,
    Uint8List parentGenesisHash,
  ) {
    _requireHash(parentGenesisHash, 'parent genesis hash');
    return _scoped(
      [op, parentGenesisHash],
      (p) => _take(
        lib
            .fn<_OpDigestC>('aviary_subchain_op_digest')
            .asFunction<_OpDigestD>()(p[0], op.length, parentChainId, p[1]),
        'subchain_op_digest',
      ),
    );
  }

  /// Signs [op] as registrar [registrarIndex] with [secret] and returns the
  /// operation with that approval added. Throws when that registrar already
  /// approved. The secret is wiped from native memory after the call.
  Uint8List signOp(
    Uint8List op, {
    required int parentChainId,
    required Uint8List parentGenesisHash,
    required int registrarIndex,
    required Uint8List secret,
    SignatureScheme scheme = SignatureScheme.ed25519,
  }) {
    _requireHash(parentGenesisHash, 'parent genesis hash');
    return _scoped(
      [op, parentGenesisHash, secret],
      (p) => _take(
        lib.fn<_OpSignC>('aviary_subchain_op_sign').asFunction<_OpSignD>()(
          p[0],
          op.length,
          parentChainId,
          p[1],
          registrarIndex,
          scheme.code,
          p[2],
          secret.length,
        ),
        'subchain_op_sign',
      ),
    );
  }

  /// Adds an approval whose signature over [opDigest] was made elsewhere.
  Uint8List addOpApproval(
    Uint8List op,
    int registrarIndex,
    Uint8List signature,
  ) => _scoped(
    [op, signature],
    (p) => _take(
      lib
          .fn<_OpApprovalC>('aviary_subchain_op_add_approval')
          .asFunction<_OpApprovalD>()(
        p[0],
        op.length,
        registrarIndex,
        p[1],
        signature.length,
      ),
      'subchain_op_add_approval',
    ),
  );
}
