part of '../../aviary.dart';

/// A membership policy: issuer key, attribute schema and tier threshold.
/// Build one with [AviaryCredentials.makeCredentialPolicy].
/// {@category Assurance and credentials}
final class CredentialPolicy {
  CredentialPolicy._(this._bc, this.encoded);

  final Aviary _bc;

  /// The policy's canonical bytes.
  final Uint8List encoded;

  /// The 32 byte commitment to commit into genesis.
  Uint8List hash() => _scoped(
    [encoded],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
        'aviary_credential_policy_hash',
      )(p[0], encoded.length),
      'credential_policy_hash',
    ),
  );

  /// Issuer side: signs a credential for a member. Holder secret.
  Uint8List issue(
    Uint8List issuerSecret,
    Uint8List memberSecret,
    int tier,
    int notAfter,
    int issuerEpoch,
  ) => _scoped(
    [encoded, issuerSecret, memberSecret],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_CredIssueC, _CredIssueD>(
        'aviary_credential_issue',
      )(
        p[0],
        encoded.length,
        p[1],
        issuerSecret.length,
        p[2],
        memberSecret.length,
        tier,
        notAfter,
        issuerEpoch,
      ),
      'credential_issue',
    ),
  );

  /// Holder side: an unlinkable presentation bound to epoch, scope and
  /// challenge.
  Uint8List present(
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
          'aviary_credential_present',
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
        'credential_present',
      ),
    ),
  );

  /// The answer to an admission challenge [wire] from a peer that holds
  /// [credential] under this policy.
  Uint8List admissionAnswer(Uint8List credential, Uint8List wire) => _scoped(
    [encoded, credential, wire],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_AdmAnswerC, _AdmAnswerD>(
        'aviary_admission_answer',
      )(p[0], encoded.length, p[1], credential.length, p[2], wire.length),
      'admission_answer',
    ),
  );
}
