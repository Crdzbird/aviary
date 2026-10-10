import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);

bool eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void main() {
  final bc = Aviary.instance;

  final issuer = bc.newBBSIssuer(fill(0x2a));
  final policy = bc.makeCredentialPolicy(
    chainId: 7,
    issuerPublic: issuer.public,
    header: Uint8List.fromList('aviary.credential.v2'.codeUnits),
    minTier: 2,
    mode: 1,
  );

  const notAfter = 4102444800;
  const now = 1700000000;

  Uint8List issue(Uint8List memberSecret, int tier, int na, int epoch) {
    final mc = policy.membershipCommit(memberSecret);
    final commitment = extractCommitment(mc);
    final bi = policy.blindIssue(issuer.secret, commitment, tier, na, epoch);
    return bc.finalizeMembership(mc, bi);
  }

  final memberSecret = fill(0x77);
  final cred = issue(memberSecret, 3, notAfter, 1);

  final gate = bc.makeCredentialGate(policy);
  Uint8List present(Uint8List c, int epoch, String scope, int ch) =>
      policy.presentPseudonymous(c, epoch, scope, fill(ch));

  final nymA = gate.admitPseudonymous(
      5, 'gossip', fill(1), present(cred, 5, 'gossip', 1), now);
  expect(
      nymA != null && nymA.length == 48, 'admit returns a 48-byte pseudonym');

  final nym2 = gate.admitPseudonymous(
      5, 'gossip', fill(2), present(cred, 5, 'gossip', 2), now);
  expect(
      nym2 != null && eq(nym2, nymA!), 'same member+epoch+scope -> same nym');
  expect(gate.noteAdmission(5, 'gossip', nymA!), 'first admission fresh');
  expect(
      !gate.noteAdmission(5, 'gossip', nym2!), 'Sybil cap flags the duplicate');

  final nymE = gate.admitPseudonymous(
      6, 'gossip', fill(1), present(cred, 6, 'gossip', 1), now);
  expect(nymE != null && !eq(nymE, nymA), 'different epoch -> different nym');
  final nymS = gate.admitPseudonymous(
      5, 'submit', fill(1), present(cred, 5, 'submit', 1), now);
  expect(nymS != null && !eq(nymS, nymA), 'different scope -> different nym');

  final nymO = gate.admitPseudonymous(5, 'gossip', fill(1),
      present(issue(fill(0x88), 3, notAfter, 1), 5, 'gossip', 1), now);
  expect(nymO != null && !eq(nymO, nymA), 'different member -> different nym');

  final presA = present(cred, 5, 'gossip', 1);
  expect(gate.admitPseudonymous(6, 'gossip', fill(1), presA, now) == null,
      'wrong epoch rejected');
  expect(gate.admitPseudonymous(5, 'submit', fill(1), presA, now) == null,
      'wrong scope rejected');
  expect(gate.admitPseudonymous(5, 'gossip', fill(9), presA, now) == null,
      'wrong challenge rejected');

  expect(
      gate.admitPseudonymous(5, 'gossip', fill(1),
              present(issue(memberSecret, 3, 1000, 1), 5, 'gossip', 1), now) ==
          null,
      'expired rejected');
  expect(
      gate.admitPseudonymous(
              5,
              'gossip',
              fill(1),
              present(issue(memberSecret, 1, notAfter, 1), 5, 'gossip', 1),
              now) ==
          null,
      'below-min-tier rejected');

  final revoking = bc.makeCredentialGate(policy);
  revoking.revokeNym(nymA);
  expect(
      revoking.admitPseudonymous(
              5, 'gossip', fill(3), present(cred, 5, 'gossip', 3), now) ==
          null,
      'revoked nym rejected');
  expect(
      revoking.admitPseudonymous(
              6, 'gossip', fill(3), present(cred, 6, 'gossip', 3), now) !=
          null,
      "a different epoch's nym is not banned");
  revoking.free();

  final rogue = bc.newBBSIssuer(fill(0x99));
  final fmc = policy.membershipCommit(memberSecret);
  final fbi =
      policy.blindIssue(rogue.secret, extractCommitment(fmc), 3, notAfter, 1);
  final forged = bc.finalizeMembership(fmc, fbi);
  expect(
      gate.admitPseudonymous(
              5, 'gossip', fill(1), present(forged, 5, 'gossip', 1), now) ==
          null,
      'forged issuer rejected');
  expect(
      gate.admitPseudonymous(5, 'gossip', fill(1),
              Uint8List.fromList('garbage'.codeUnits), now) ==
          null,
      'malformed rejected');

  gate.free();
  print('aviary dart credential-gate pseudonymous smoke: OK '
      '(blind issuance, per-epoch pseudonym, Sybil cap, revocation)');
}
