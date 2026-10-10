import 'dart:async';
import 'dart:io';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Uint8List frame(Uint8List payload) {
  final out = BytesBuilder()
    ..add((ByteData(4)..setUint32(0, payload.length)).buffer.asUint8List())
    ..add(payload);
  return out.toBytes();
}

Stream<Uint8List> frames(Socket socket) async* {
  var pending = Uint8List(0);
  await for (final chunk in socket) {
    final data = Uint8List.fromList([...pending, ...chunk]);
    var off = 0;
    while (data.length - off >= 4) {
      final len = ByteData.sublistView(data, off, off + 4).getUint32(0);
      if (data.length - off - 4 < len) break;
      yield Uint8List.sublistView(data, off + 4, off + 4 + len);
      off += 4 + len;
    }
    pending = Uint8List.sublistView(data, off);
  }
}

Future<void> waitUntil(bool Function() done, String what) async {
  for (var i = 0; i < 1000; i++) {
    if (done()) return;
    await Future<void>.delayed(const Duration(milliseconds: 10));
  }
  throw StateError('FAIL: timed out waiting for $what');
}

Future<void> main() async {
  final bc = Aviary.instance;
  final keys = [bc.newValidator(), bc.newValidator()];
  final ids = [bc.generateSecureIdentity(), bc.generateSecureIdentity()];
  final alice = Uint8List(32)..fillRange(0, 32, 0x22);
  final genesis = bc.buildGenesis(
    chainId: 7,
    accounts: [(address: alice, balance: 50)],
    validators: [keys[0].entry(1), keys[1].entry(1)],
  );
  final nodes = [
    bc.newNetNode(genesis: genesis, me: 0, blsKey: keys[0], peers: [1]),
    bc.newNetNode(genesis: genesis, me: 1, blsKey: keys[1], peers: [0]),
  ];

  final server = await ServerSocket.bind(InternetAddress.loopbackIPv4, 0);
  final accepting = server.first;
  final dialer =
      await Socket.connect(InternetAddress.loopbackIPv4, server.port);
  final acceptor = await accepting;
  final sockets = [dialer, acceptor];
  final readers = [
    StreamIterator(frames(dialer)),
    StreamIterator(frames(acceptor))
  ];

  final init = bc.initiateSecureSession(ids[1].kem.public, ids[0].sig);
  dialer.add(frame(init.handshakeMessage));
  expect(await readers[1].moveNext(), 'responder receives msg1');
  final resp = bc.acceptSecureSession(ids[1], readers[1].current);
  acceptor.add(frame(resp.handshakeMessage));
  expect(await readers[0].moveNext(), 'initiator receives msg2');
  init.complete(readers[0].current);
  final sessions = [init, resp];
  expect(sessions.every((s) => s.isOpen), 'both ends open');
  expect(init.peerIdentity.equalsBytes(ids[1].sig.public), 'peer 1 pinned');
  expect(resp.peerIdentity.equalsBytes(ids[0].sig.public), 'peer 0 pinned');

  var stopped = false;
  void apply(int i, Outbound out) {
    for (final r in out.relays) {
      expect(r.to == 1 - i, 'relays go to the only peer');
      sockets[i].add(frame(sessions[i].encrypt(r.message)));
    }
    for (final t in out.timeouts) {
      Timer(Duration(milliseconds: 200 * (t.round + 1)), () {
        if (stopped) return;
        apply(i, nodes[i].onTimeout(t));
      });
    }
  }

  Future<void> pump(int i) async {
    while (await readers[i].moveNext()) {
      if (stopped) return;
      apply(
          i, nodes[i].onGossip(1 - i, sessions[i].decrypt(readers[i].current)));
    }
  }

  final pumps = [pump(0), pump(1)];
  for (var i = 0; i < 2; i++) {
    apply(i, nodes[i].start());
  }
  await waitUntil(() => nodes.every((n) => n.committed), 'height 1');
  expect(nodes.every((n) => n.height == 1), 'both at height 1 over TCP');
  for (var i = 0; i < 2; i++) {
    apply(i, nodes[i].advance());
  }
  await waitUntil(() => nodes.every((n) => n.height == 2), 'height 2');
  expect(nodes[0].tip().equalsBytes(nodes[1].tip()), 'identical tips');
  expect(nodes[1].balance(alice) == 50, 'state intact');

  stopped = true;
  for (final s in sockets) {
    await s.close();
    s.destroy();
  }
  await server.close();
  await Future.wait(pumps);
  for (final n in nodes) {
    n.free();
  }
  for (final s in sessions) {
    s.free();
  }
  print('aviary dart network tcp smoke: OK');
}
