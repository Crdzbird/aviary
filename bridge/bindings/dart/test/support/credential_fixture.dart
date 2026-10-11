import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';

/// An issuer, a tier 2 policy and one tier 3 credential for a member.
final class CredentialFixture {
  CredentialFixture(this.bc, {this.pseudonymous = false})
    : issuer = bc.newBBSIssuer(fill(pseudonymous ? 0x2a : 0x11)) {
    policy = bc.makeCredentialPolicy(
      chainId: 7,
      issuerPublic: issuer.public,
      header: Uint8List.fromList(
        'aviary.credential.v${pseudonymous ? 2 : 1}'.codeUnits,
      ),
      minTier: 2,
      mode: pseudonymous ? 1 : 0,
    );
    gate = bc.makeCredentialGate(policy);
  }

  static const notAfter = 4102444800;
  static const now = 1700000000;

  final Aviary bc;
  final bool pseudonymous;
  final KeyPairBytes issuer;
  final Uint8List memberSecret = fill(0x77);
  late final CredentialPolicy policy;
  late final CredentialGate gate;

  /// A credential issued directly by [issuerSecret] (default: ours).
  Uint8List issue({
    Uint8List? issuerSecret,
    Uint8List? member,
    int tier = 3,
    int notAfter = notAfter,
  }) => policy.issue(
    issuerSecret ?? issuer.secret,
    member ?? memberSecret,
    tier,
    notAfter,
    1,
  );

  /// A credential issued blind: the member commits, the issuer signs the
  /// commitment, the member finalizes.
  Uint8List issueBlind({
    Uint8List? issuerSecret,
    Uint8List? member,
    int tier = 3,
    int notAfter = notAfter,
  }) {
    final mc = policy.membershipCommit(member ?? memberSecret);
    final bi = policy.blindIssue(
      issuerSecret ?? issuer.secret,
      extractCommitment(mc),
      tier,
      notAfter,
      1,
    );
    return bc.finalizeMembership(mc, bi);
  }

  /// A pseudonymous presentation of [cred] under challenge byte [ch].
  Uint8List present(Uint8List cred, int epoch, String scope, int ch) =>
      policy.presentPseudonymous(cred, epoch, scope, fill(ch));

  /// Admits [cred] pseudonymously and returns the nym, or null.
  Uint8List? admit(Uint8List cred, int epoch, String scope, [int ch = 1]) =>
      gate.admitPseudonymous(
        epoch,
        scope,
        fill(ch),
        present(cred, epoch, scope, ch),
        now,
      );
}
