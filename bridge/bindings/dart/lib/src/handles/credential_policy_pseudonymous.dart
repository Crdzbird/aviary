part of '../../aviary.dart';

/// The pseudonymous issuance and presentation flow of a policy.
extension CredentialPolicyPseudonymous on CredentialPolicy {
  /// Holder step one: a blinded membership commit. Holder secret; send
  /// [extractCommitment] of it to the issuer.
  Uint8List membershipCommit(Uint8List memberSecret) => _scoped(
    [encoded, memberSecret],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_Buf2C, _Buf2D>('aviary_membership_commit')(
        p[0],
        encoded.length,
        p[1],
        memberSecret.length,
      ),
      'membership_commit',
    ),
  );

  /// Issuer step two: blind signs the holder's [commitment].
  Uint8List blindIssue(
    Uint8List issuerSecret,
    Uint8List commitment,
    int tier,
    int notAfter,
    int issuerEpoch,
  ) => _scoped(
    [encoded, issuerSecret, commitment],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_BlindIssueC, _BlindIssueD>('aviary_blind_issue')(
        p[0],
        encoded.length,
        p[1],
        issuerSecret.length,
        p[2],
        commitment.length,
        tier,
        notAfter,
        issuerEpoch,
      ),
      'blind_issue',
    ),
  );

  /// Holder step three: a per epoch pseudonymous presentation.
  Uint8List presentPseudonymous(
    Uint8List credential,
    int epoch,
    String scope,
    Uint8List challenge,
  ) => _withUtf8(
    scope,
    (sp) => _scoped(
      [encoded, credential, challenge],
      (p) => _bc._take(
        _bc.lib.lookupFunction<_CredPresentC, _CredPresentD>(
          'aviary_credential_present_pseudonymous',
        )(
          p[0],
          encoded.length,
          p[1],
          credential.length,
          epoch,
          sp,
          p[2],
          challenge.length,
        ),
        'credential_present_pseudonymous',
      ),
    ),
  );
}
