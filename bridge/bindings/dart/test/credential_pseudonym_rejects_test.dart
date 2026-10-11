import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/credential_fixture.dart';

void main() {
  test('credential pseudonym rejects', () {
    final bc = Aviary.instance;
    final fx = CredentialFixture(bc, pseudonymous: true);
    final gate = fx.gate;
    const now = CredentialFixture.now;
    final cred = fx.issueBlind();
    final presA = fx.present(cred, 5, 'gossip', 1);
    check(
      gate.admitPseudonymous(6, 'gossip', fill(1), presA, now) == null,
      'wrong epoch rejected',
    );
    check(
      gate.admitPseudonymous(5, 'submit', fill(1), presA, now) == null,
      'wrong scope rejected',
    );
    check(
      gate.admitPseudonymous(5, 'gossip', fill(9), presA, now) == null,
      'wrong challenge rejected',
    );
    check(
      fx.admit(fx.issueBlind(notAfter: 1000), 5, 'gossip') == null,
      'expired rejected',
    );
    check(
      fx.admit(fx.issueBlind(tier: 1), 5, 'gossip') == null,
      'below-min-tier rejected',
    );
    final revoking = bc.makeCredentialGate(fx.policy);
    final nymA = fx.admit(cred, 5, 'gossip')!;
    revoking.revokeNym(nymA);
    check(
      revoking.admitPseudonymous(
            5,
            'gossip',
            fill(3),
            fx.present(cred, 5, 'gossip', 3),
            now,
          ) ==
          null,
      'revoked nym rejected',
    );
    check(
      revoking.admitPseudonymous(
            6,
            'gossip',
            fill(3),
            fx.present(cred, 6, 'gossip', 3),
            now,
          ) !=
          null,
      "a different epoch's nym is not banned",
    );
    revoking.free();
    final rogue = bc.newBBSIssuer(fill(0x99));
    check(
      fx.admit(fx.issueBlind(issuerSecret: rogue.secret), 5, 'gossip') == null,
      'forged issuer rejected',
    );
    check(
      gate.admitPseudonymous(
            5,
            'gossip',
            fill(1),
            Uint8List.fromList('garbage'.codeUnits),
            now,
          ) ==
          null,
      'malformed rejected',
    );
    gate.free();
    print('aviary dart credential pseudonym rejects smoke: OK');
  });
}
