part of '../../blockchainer.dart';

/// The pseudonymous admission flow of a gate.
extension CredentialGatePseudonymous on CredentialGate {
  /// Admits a pseudonymous presentation and returns the 48 byte pseudonym,
  /// or null on any reject. Never throws.
  Uint8List? admitPseudonymous(
    int epoch,
    String scope,
    Uint8List challenge,
    Uint8List presentation,
    int now,
  ) =>
      _withUtf8(
        scope,
        (sp) => _scoped([challenge, presentation], (p) {
          final nym = _bc._take(
            _bc.lib.lookupFunction<_CredGateAdmitPseudC, _CredGateAdmitPseudD>(
                    'blockchainer_credential_gate_admit_pseudonymous')(
                _h,
                epoch,
                sp,
                p[0],
                challenge.length,
                p[1],
                presentation.length,
                now),
            'admit_pseudonymous',
          );
          if (nym.isEmpty) return null;
          return nym;
        }),
      );

  /// Targeted revocation of an exact pseudonym.
  void revokeNym(Uint8List nym) => _scoped(
        [nym],
        (p) => _bc.lib.lookupFunction<_CredGateRevokeNymC, _CredGateRevokeNymD>(
            'blockchainer_credential_gate_revoke_nym')(_h, p[0], nym.length),
      );

  /// Records an admitted [nym]; true when it is the first for this epoch and
  /// scope.
  bool noteAdmission(int epoch, String scope, Uint8List nym) => _withUtf8(
        scope,
        (sp) => _scoped(
          [nym],
          (p) =>
              _bc.lib.lookupFunction<_CredGateNoteAdmC, _CredGateNoteAdmD>(
                      'blockchainer_credential_gate_note_admission')(
                  _h, epoch, sp, p[0], nym.length) ==
              1,
        ),
      );
}
