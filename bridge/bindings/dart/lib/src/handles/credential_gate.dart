part of '../../aviary.dart';

/// An admission challenge: keep [nonce], send [wire] to the peer.
/// {@category Networking}
typedef AdmissionChallenge = ({Uint8List nonce, Uint8List wire});

/// A verifier's anonymous membership admission gate. It admits a holder by a
/// zero knowledge proof of an issuer signed credential clearing the policy.
/// {@category Assurance and credentials}
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

  /// A fresh admission challenge for [epoch]: keep the nonce, send the wire
  /// message to the connecting peer.
  AdmissionChallenge admissionChallenge(int epoch) {
    final f = _bc.lib.lookupFunction<_AdmChalC, _AdmChalD>(
      'aviary_admission_challenge',
    );
    final b = _bc._take(f(epoch), 'admission_challenge');
    return (nonce: b.sublist(0, 32), wire: b.sublist(32));
  }

  /// Whether a peer's [presentation], answering the challenge whose [nonce]
  /// this gate issued ([admissionChallenge]), admits for [epoch] at [now].
  /// Never throws.
  bool admissionGranted(
    int epoch,
    Uint8List nonce,
    Uint8List presentation,
    int now,
  ) => _scoped([nonce, presentation], (p) {
    final f = _bc.lib.lookupFunction<_AdmGrantC, _AdmGrantD>(
      'aviary_admission_granted',
    );
    return f(_h, epoch, p[0], nonce.length, p[1], presentation.length, now) ==
        1;
  });

  /// Releases the native gate.
  void free() => _release();
}
