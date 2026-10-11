import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/credential_fixture.dart';

void main() {
  test('credential gate', () {
    final fx = CredentialFixture(Aviary.instance);
    check(
      fx.issuer.public.length == 96 && fx.issuer.secret.length == 32,
      'BBS issuer key sizes',
    );
    check(fx.policy.hash().length == 32, 'policy hash is 32 bytes');
    final cred = fx.issue();
    final gate = fx.gate;
    final challenge = fill(0x01);
    const now = CredentialFixture.now;
    final pres = fx.policy.present(cred, 5, 'submit', challenge);
    check(
      gate.admit(5, 'submit', challenge, pres, now),
      'valid presentation admitted',
    );
    final pres2 = fx.policy.present(cred, 5, 'submit', challenge);
    check(
      !pres.equalsBytes(pres2),
      'two presentations of one credential are unlinkable',
    );
    check(
      gate.admit(5, 'submit', challenge, pres2, now),
      'second presentation admitted',
    );
    check(
      !gate.admit(6, 'submit', challenge, pres, now),
      'wrong epoch rejected',
    );
    check(
      !gate.admit(5, 'gossip', challenge, pres, now),
      'wrong scope rejected',
    );
    check(
      !gate.admit(5, 'submit', fill(0x02), pres, now),
      'wrong challenge rejected',
    );
    gate.free();
    print('aviary dart credential-gate smoke: OK');
  });
}
