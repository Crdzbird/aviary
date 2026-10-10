import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void check(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);

void main() {
  test('credential gate', () {
    final bc = Aviary.instance;

    final issuer = bc.newBBSIssuer(fill(0x11));
    check(
      issuer.public.length == 96 && issuer.secret.length == 32,
      'BBS issuer key sizes',
    );

    final policy = bc.makeCredentialPolicy(
      chainId: 7,
      issuerPublic: issuer.public,
      header: Uint8List.fromList('aviary.credential.v1'.codeUnits),
      minTier: 2,
    );
    check(policy.hash().length == 32, 'policy hash is 32 bytes');

    final memberSecret = fill(0xAB);
    const notAfter = 4102444800;
    const now = 1700000000;
    final cred = policy.issue(issuer.secret, memberSecret, 3, notAfter, 1);

    final gate = bc.makeCredentialGate(policy);
    final challenge = fill(0x01);

    final pres = policy.present(cred, 5, 'submit', challenge);
    check(
      gate.admit(5, 'submit', challenge, pres, now),
      'valid presentation admitted',
    );

    final pres2 = policy.present(cred, 5, 'submit', challenge);
    check(
      !_eq(pres, pres2),
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

    final ep = policy.present(
      policy.issue(issuer.secret, memberSecret, 3, 1000, 1),
      5,
      'submit',
      challenge,
    );
    check(
      !gate.admit(5, 'submit', challenge, ep, now),
      'expired credential rejected',
    );

    final lp = policy.present(
      policy.issue(issuer.secret, memberSecret, 1, notAfter, 1),
      5,
      'submit',
      challenge,
    );
    check(
      !gate.admit(5, 'submit', challenge, lp, now),
      'below-min-tier rejected',
    );

    final revoking = bc.makeCredentialGate(policy);
    revoking.revokeIssuerEpoch(1);
    check(
      !revoking.admit(5, 'submit', challenge, pres, now),
      'revoked issuer epoch rejected',
    );
    revoking.free();

    final rogue = bc.newBBSIssuer(fill(0x77));
    final fp = policy.present(
      policy.issue(rogue.secret, memberSecret, 3, notAfter, 1),
      5,
      'submit',
      challenge,
    );
    check(
      !gate.admit(5, 'submit', challenge, fp, now),
      'forged (wrong-issuer) credential rejected',
    );

    check(
      !gate.admit(
        5,
        'submit',
        challenge,
        Uint8List.fromList('garbage'.codeUnits),
        now,
      ),
      'malformed presentation rejected',
    );

    gate.free();
    print(
      'aviary dart credential-gate smoke: OK '
      '(unlinkable admission, context-binding, expiry, tier, revocation)',
    );
  });
}

bool _eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}
