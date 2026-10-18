part of '../../aviary.dart';

/// Anonymous membership credentials: issuers, policies and gates.
/// {@category Assurance and credentials}
extension AviaryCredentials on Aviary {
  /// A BBS issuer key pair from at least 32 bytes of key material.
  KeyPairBytes newBBSIssuer(Uint8List keyMaterial) => _scoped(
    [keyMaterial],
    (p) => _splitKeyBlob(
      _take(
        lib.fn<_BufOfBytesC>('aviary_bbs_keygen').asFunction<_BufOfBytesD>()(
          p[0],
          keyMaterial.length,
        ),
        'bbs_keygen',
      ),
    ),
  );

  /// Canonically encodes a membership policy. [mode] 0 is unlinkable, 1 is
  /// pseudonymous.
  CredentialPolicy makeCredentialPolicy({
    required int chainId,
    required Uint8List issuerPublic,
    required Uint8List header,
    int attributeCount = 4,
    int minTier = 0,
    int mode = 0,
  }) => _scoped([issuerPublic, header], (p) {
    final enc = mode == 0
        ? lib
              .fn<_CredPolicyEncodeC>('aviary_credential_policy_encode')
              .asFunction<_CredPolicyEncodeD>()(
            chainId,
            p[0],
            issuerPublic.length,
            p[1],
            header.length,
            attributeCount,
            minTier,
          )
        : lib
              .fn<_CredPolicyEncodeExC>('aviary_credential_policy_encode_ex')
              .asFunction<_CredPolicyEncodeExD>()(
            chainId,
            p[0],
            issuerPublic.length,
            p[1],
            header.length,
            attributeCount,
            minTier,
            mode,
          );
    return CredentialPolicy._(this, _take(enc, 'credential_policy_encode'));
  });

  /// Opens a verifier's admission gate enforcing [policy].
  CredentialGate makeCredentialGate(CredentialPolicy policy) =>
      _scoped([policy.encoded], (p) {
        final h = lib
            .fn<_CredGateNewC>('aviary_credential_gate_new')
            .asFunction<_CredGateNewD>()(p[0], policy.encoded.length);
        return CredentialGate._(
          this,
          _requireHandle(h, 'credential gate creation'),
        );
      });

  /// Pseudonymous holder step three: the finished credential. Holder secret.
  Uint8List finalizeMembership(
    Uint8List membershipCommit,
    Uint8List blindIssuance,
  ) => _scoped(
    [membershipCommit, blindIssuance],
    (p) => _take(
      lib.fn<_Buf2C>('aviary_finalize_membership').asFunction<_Buf2D>()(
        p[0],
        membershipCommit.length,
        p[1],
        blindIssuance.length,
      ),
      'finalize_membership',
    ),
  );
}
