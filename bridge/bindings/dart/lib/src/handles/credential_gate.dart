part of '../../aviary.dart';

/// A verifier's anonymous membership admission gate. It admits a holder by a
/// zero knowledge proof of an issuer signed credential clearing the policy.
final class CredentialGate extends _NativeHandle {
  CredentialGate._(Aviary bc, Pointer<Void> h)
    : super(bc, h, 'aviary_credential_gate_free');

  /// Bulk revocation: refuses every credential issued in [epoch].
  void revokeIssuerEpoch(int epoch) =>
      _bc.lib.lookupFunction<_CredGateRevokeC, _CredGateRevokeD>(
        'aviary_credential_gate_revoke_epoch',
      )(_h, epoch);

  /// Runs the full admission checklist. A malformed presentation is false;
  /// this never throws.
  bool admit(
    int epoch,
    String scope,
    Uint8List challenge,
    Uint8List presentation,
    int now,
  ) => _withUtf8(
    scope,
    (sp) => _scoped(
      [challenge, presentation],
      (p) =>
          _bc.lib.lookupFunction<_CredGateAdmitC, _CredGateAdmitD>(
            'aviary_credential_gate_admit',
          )(
            _h,
            epoch,
            sp,
            p[0],
            challenge.length,
            p[1],
            presentation.length,
            now,
          ) ==
          1,
    ),
  );

  /// Releases the native gate.
  void free() => _release();
}
