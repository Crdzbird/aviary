import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/credential_fixture.dart';

void main() {
  test('credential gate rejects', () {
    final bc = Aviary.instance;
    final fx = CredentialFixture(bc);
    final gate = fx.gate;
    final challenge = fill(0x01);
    const now = CredentialFixture.now;
    Uint8List present(Uint8List cred) =>
        fx.policy.present(cred, 5, 'submit', challenge);
    bool admit(Uint8List pres) => gate.admit(5, 'submit', challenge, pres, now);
    check(!admit(present(fx.issue(notAfter: 1000))), 'expired rejected');
    check(!admit(present(fx.issue(tier: 1))), 'below-min-tier rejected');
    final revoking = bc.makeCredentialGate(fx.policy);
    revoking.revokeIssuerEpoch(1);
    check(
      !revoking.admit(5, 'submit', challenge, present(fx.issue()), now),
      'revoked issuer epoch rejected',
    );
    revoking.free();
    final rogue = bc.newBBSIssuer(fill(0x99));
    check(
      !admit(present(fx.issue(issuerSecret: rogue.secret))),
      'forged (wrong-issuer) credential rejected',
    );
    check(
      !admit(Uint8List.fromList('garbage'.codeUnits)),
      'malformed presentation rejected',
    );
    gate.free();
    print('aviary dart credential-gate rejects smoke: OK');
  });
}
