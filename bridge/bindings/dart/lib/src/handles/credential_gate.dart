part of '../../aviary.dart';

/// A verifier's anonymous membership admission gate. It admits a holder by a
/// zero knowledge proof of an issuer signed credential clearing the policy.
final class CredentialGate extends _NativeHandle {
  CredentialGate._(Aviary bc, Pointer<Void> h)
      : super(bc, h, 'aviary_credential_gate_free');

  /// Bulk revocation: refuses every credential issued in [epoch].
  void revokeIssuerEpoch(int epoch) =>
      _bc.lib.lookupFunction<_CredGateRevokeC, _CredGateRevokeD>(
          'aviary_credential_gate_revoke_epoch')(_h, epoch);

  /// Runs the full admission checklist. A malformed presentation is false;
  /// this never throws.
  bool admit(
    int epoch,
    String scope,
    Uint8List challenge,
    Uint8List presentation,
    int now,
  ) =>
      _withUtf8(
        scope,
        (sp) => _scoped(
          [challenge, presentation],
          (p) =>
              _bc.lib.lookupFunction<_CredGateAdmitC, _CredGateAdmitD>(
                      'aviary_credential_gate_admit')(_h, epoch, sp, p[0],
                  challenge.length, p[1], presentation.length, now) ==
              1,
        ),
      );

  /// Releases the native gate.
  void free() => _release();
}

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
                    'aviary_credential_gate_admit_pseudonymous')(_h, epoch, sp,
                p[0], challenge.length, p[1], presentation.length, now),
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
            'aviary_credential_gate_revoke_nym')(_h, p[0], nym.length),
      );

  /// Records an admitted [nym]; true when it is the first for this epoch and
  /// scope.
  bool noteAdmission(int epoch, String scope, Uint8List nym) => _withUtf8(
        scope,
        (sp) => _scoped(
          [nym],
          (p) =>
              _bc.lib.lookupFunction<_CredGateNoteAdmC, _CredGateNoteAdmD>(
                      'aviary_credential_gate_note_admission')(
                  _h, epoch, sp, p[0], nym.length) ==
              1,
        ),
      );
}
