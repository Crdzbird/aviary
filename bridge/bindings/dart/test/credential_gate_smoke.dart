// CredentialGate — anonymous membership admission (Option A, unlinkable) over the
// Dart binding. An issuer signs a member's attributes as a BBS credential; a
// holder admits itself to a role by proving in zero knowledge it holds an
// issuer-signed credential whose tier clears the policy and whose expiry has not
// passed — revealing nothing else.
import 'dart:typed_data';
import 'package:blockchainer_dart/blockchainer.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);

void main() {
  final bc = Blockchainer.instance;

  final issuer = bc.newBBSIssuer(fill(0x11));
  expect(issuer.public.length == 96 && issuer.secret.length == 32, 'BBS issuer key sizes');

  final policy = bc.makeCredentialPolicy(
    chainId: 7,
    issuerPublic: issuer.public,
    header: Uint8List.fromList('blockchainer.credential.v1'.codeUnits),
    minTier: 2,
  );
  expect(policy.hash().length == 32, 'policy hash is 32 bytes');

  final memberSecret = fill(0xAB);
  const notAfter = 4102444800; // year 2100
  const now = 1700000000;
  final cred = policy.issue(issuer.secret, memberSecret, 3, notAfter, 1);

  final gate = bc.makeCredentialGate(policy);
  final challenge = fill(0x01);

  // Happy path.
  final pres = policy.present(cred, 5, 'submit', challenge);
  expect(gate.admit(5, 'submit', challenge, pres, now), 'valid presentation admitted');

  // Unlinkability: a fresh presentation differs on the wire yet still admits.
  final pres2 = policy.present(cred, 5, 'submit', challenge);
  expect(!_eq(pres, pres2), 'two presentations of one credential are unlinkable');
  expect(gate.admit(5, 'submit', challenge, pres2, now), 'second presentation admitted');

  // Context binding (anti-replay).
  expect(!gate.admit(6, 'submit', challenge, pres, now), 'wrong epoch rejected');
  expect(!gate.admit(5, 'gossip', challenge, pres, now), 'wrong scope rejected');
  expect(!gate.admit(5, 'submit', fill(0x02), pres, now), 'wrong challenge rejected');

  // Expiry.
  final ep = policy.present(policy.issue(issuer.secret, memberSecret, 3, 1000, 1), 5, 'submit', challenge);
  expect(!gate.admit(5, 'submit', challenge, ep, now), 'expired credential rejected');

  // Tier below the policy threshold.
  final lp = policy.present(policy.issue(issuer.secret, memberSecret, 1, notAfter, 1), 5, 'submit', challenge);
  expect(!gate.admit(5, 'submit', challenge, lp, now), 'below-min-tier rejected');

  // Bulk revocation.
  final revoking = bc.makeCredentialGate(policy);
  revoking.revokeIssuerEpoch(1);
  expect(!revoking.admit(5, 'submit', challenge, pres, now), 'revoked issuer epoch rejected');
  revoking.free();

  // Forgery: a credential from another issuer.
  final rogue = bc.newBBSIssuer(fill(0x77));
  final fp = policy.present(policy.issue(rogue.secret, memberSecret, 3, notAfter, 1), 5, 'submit', challenge);
  expect(!gate.admit(5, 'submit', challenge, fp, now), 'forged (wrong-issuer) credential rejected');

  // Malformed presentation is a quiet reject.
  expect(!gate.admit(5, 'submit', challenge, Uint8List.fromList('garbage'.codeUnits), now),
      'malformed presentation rejected');

  gate.free();
  print('blockchainer dart credential-gate smoke: OK '
      '(unlinkable admission, context-binding, expiry, tier, revocation)');
}

bool _eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}
