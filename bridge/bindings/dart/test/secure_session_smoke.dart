import 'package:aviary_dart/aviary.dart';

import 'support/check.dart';

void main() {
  secure(Aviary.instance);
  print('aviary dart secure session smoke: OK');
}

void secure(Aviary bc) {
  final a = bc.generateSecureIdentity();
  final b = bc.generateSecureIdentity();
  final init = bc.initiateSecureSession(b.kem.public, a.sig);
  expect(!init.isOpen, 'initiator waits for the reply');
  final resp = bc.acceptSecureSession(b, init.handshakeMessage);
  expect(resp.isOpen, 'responder opens on msg1');
  expect(resp.peerIdentity.equalsBytes(a.sig.public), 'initiator pinned');
  init.complete(resp.handshakeMessage);
  expect(init.isOpen, 'initiator opens on msg2');
  expect(init.peerIdentity.equalsBytes(b.sig.public), 'responder pinned');
  final hello = 'hello'.utf8Bytes;
  expect(resp.decrypt(init.encrypt(hello)).equalsBytes(hello), 'a to b');
  expect(init.decrypt(resp.encrypt(hello)).equalsBytes(hello), 'b to a');
  final frame = init.encrypt(hello);
  frame[frame.length - 1] ^= 1;
  var tampered = false;
  try {
    resp.decrypt(frame);
  } on AviaryException catch (_) {
    tampered = true;
  }
  expect(tampered, 'tampered frame refused');
  var wrong = false;
  try {
    bc.acceptSecureSession(
      a,
      bc.initiateSecureSession(b.kem.public, a.sig).handshakeMessage,
    );
  } on AviaryException catch (e) {
    wrong = e.status == AviaryStatus.badInput;
  }
  expect(wrong, 'wrong responder identity refused');
  init.free();
  resp.free();
}
