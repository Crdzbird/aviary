import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';
import 'support/credential_fixture.dart';

void main() {
  test('noise sessions', () {
    final bc = Aviary.instance;
    final a = bc.generateNoiseIdentity();
    final b = bc.generateNoiseIdentity();
    final prologue = utf8.encode('aviary');
    final init = bc.initiateNoiseSession(a, prologue: prologue);
    final resp = bc.acceptNoiseSession(
      b,
      init.handshakeMessage,
      prologue: prologue,
    );
    resp.completeResponder(init.completeInitiator(resp.handshakeMessage));
    check(failsAnyhow(() => init.encrypt(Uint8List(1))), 'closed before proof');
    resp.verifyIdentityProof(init.identityProof());
    init.verifyIdentityProof(resp.identityProof());
    check(init.isOpen && resp.isOpen, 'open after both proofs');
    check(init.peerIdentity.equalsBytes(b.sig.public), 'pinned identity');
    check(resp.peerStatic.equalsBytes(a.noiseStatic.public), 'peer static');
    check(init.handshakeHash.equalsBytes(resp.handshakeHash), 'same hash');
    final record = init.encrypt(utf8.encode('hello'));
    check(utf8.decode(resp.decrypt(record)) == 'hello', 'round trip');
    final bad = Uint8List.fromList(record)..[record.length - 1] ^= 1;
    check(failsAnyhow(() => resp.decrypt(bad)), 'tamper refused');
    init.free();
    resp.free();
    print('aviary dart noise smoke: OK');
  });

  test('admission handshake', () {
    final fx = CredentialFixture(Aviary.instance);
    final cred = fx.issue();
    final challenge = fx.gate.admissionChallenge(5);
    final answer = fx.policy.admissionAnswer(cred, challenge.wire);
    const now = CredentialFixture.now;
    check(
      fx.gate.admissionGranted(5, challenge.nonce, answer, now),
      'a member is admitted',
    );
    check(
      !fx.gate.admissionGranted(6, challenge.nonce, answer, now),
      'wrong epoch refused',
    );
    check(
      !fx.gate.admissionGranted(5, Uint8List(32), answer, now),
      'another challenge refused',
    );
    check(
      failsAnyhow(() => fx.policy.admissionAnswer(cred, Uint8List(2))),
      'a malformed challenge throws',
    );
    fx.gate.free();
    print('aviary dart admission smoke: OK');
  });
}
