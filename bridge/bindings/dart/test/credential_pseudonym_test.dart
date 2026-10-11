import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/credential_fixture.dart';

void main() {
  test('credential pseudonym', () {
    final fx = CredentialFixture(Aviary.instance, pseudonymous: true);
    final cred = fx.issueBlind();
    final nymA = fx.admit(cred, 5, 'gossip', 1);
    check(nymA != null && nymA.length == 48, 'admit returns a 48-byte nym');
    final nym2 = fx.admit(cred, 5, 'gossip', 2);
    check(
      nym2 != null && nym2.equalsBytes(nymA!),
      'same member+epoch+scope -> same nym',
    );
    check(fx.gate.noteAdmission(5, 'gossip', nymA!), 'first admission fresh');
    check(
      !fx.gate.noteAdmission(5, 'gossip', nym2!),
      'Sybil cap flags the duplicate',
    );
    final nymE = fx.admit(cred, 6, 'gossip', 1);
    check(
      nymE != null && !nymE.equalsBytes(nymA),
      'different epoch -> different nym',
    );
    final nymS = fx.admit(cred, 5, 'submit', 1);
    check(
      nymS != null && !nymS.equalsBytes(nymA),
      'different scope -> different nym',
    );
    final nymO = fx.admit(fx.issueBlind(member: fill(0x88)), 5, 'gossip', 1);
    check(
      nymO != null && !nymO.equalsBytes(nymA),
      'different member -> different nym',
    );
    fx.gate.free();
    print('aviary dart credential pseudonym smoke: OK');
  });
}
