import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

/// A deterministic in memory mesh: relays are queued, timeouts fire only
/// when the queue is empty, earliest round first.
final class Mesh {
  Mesh(this.nodes);

  final List<NetNode> nodes;
  final List<(int from, int to, Uint8List payload)> queue = [];
  final List<(int round, int step, int node)> timeouts = [];

  void apply(int node, Outbound out) {
    for (final r in out.relays) {
      queue.add((node, r.to, r.message));
    }
    for (final t in out.timeouts) {
      timeouts.add((t.round, t.step.code, node));
    }
  }

  void run(bool Function() done, {int budget = 20000}) {
    for (var step = 0; step < budget; step++) {
      if (done()) return;
      if (queue.isNotEmpty) {
        final (from, to, payload) = queue.removeAt(0);
        apply(to, nodes[to].onGossip(from, payload));
        continue;
      }
      if (timeouts.isEmpty) return;
      timeouts.sort((a, b) => a.$1 != b.$1 ? a.$1 - b.$1 : a.$2 - b.$2);
      final (round, step, node) = timeouts.removeAt(0);
      final request = (step: ConsensusStep.fromCode(step), round: round);
      apply(node, nodes[node].onTimeout(request));
    }
  }
}

void main() {
  final bc = Aviary.instance;
  mesh(bc);
  secure(bc);
  print('aviary dart network smoke: OK');
}

void mesh(Aviary bc) {
  final keys = List.generate(4, (_) => bc.newValidator());
  final alice = Uint8List(32)..fillRange(0, 32, 0x11);
  final genesis = bc.buildGenesis(
    chainId: 42,
    accounts: [(address: alice, balance: 1000)],
    validators: [for (final k in keys) k.entry(1)],
  );
  final nodes = [
    for (var i = 0; i < 4; i++)
      bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
    bc.newNetNode(genesis: genesis, me: AviaryNetwork.observer, peers: [3]),
  ];
  for (final (a, b) in [(0, 1), (1, 2), (2, 3), (3, 4)]) {
    nodes[a].addPeer(b);
    nodes[b].addPeer(a);
  }
  final net = Mesh(nodes);
  bool validatorsCommitted() => nodes.take(4).every((n) => n.committed);
  for (var i = 0; i < nodes.length; i++) {
    net.apply(i, nodes[i].start());
  }
  net.run(validatorsCommitted);
  expect(validatorsCommitted(), 'line mesh commits through relays');
  net.run(() => false);
  expect(nodes.every((n) => n.height == 1), 'every node incl. observer at 1');
  expect(nodes[0].tip().equalsBytes(nodes[4].tip()), 'observer has the tip');
  expect(nodes[3].balance(alice) == 1000, 'genesis balance visible');
  expect(nodes[0].replayLog().isNotEmpty, 'catch-up log holds the height');
  expect(nodes[0].onGossip(1, Uint8List.fromList([9, 9, 9])).relays.isEmpty,
      'garbage relays nothing');
  expect(!nodes[0].peerBanned(1), 'one bad payload does not ban');
  expect(
      bc.verifyStateProof(nodes[2].stateRoot(), nodes[2].proveAccount(alice)),
      'proof from a networked node verifies');
  for (var i = 0; i < nodes.length; i++) {
    net.apply(i, nodes[i].advance());
  }
  expect(nodes[3].replayLog().isEmpty, 'advance restarts a follower log');
  expect(nodes[0].replayLog().isNotEmpty, 'proposer logs its new proposal');
  expect(!nodes[0].committed, 'new height is open');
  net.run(validatorsCommitted);
  net.run(() => false);
  expect(nodes.every((n) => n.height == 2), 'second height committed');
  var refused = false;
  try {
    nodes[0].setTransactions([
      Uint8List.fromList([1, 2])
    ]);
  } on AviaryException catch (e) {
    refused = e.status == AviaryStatus.badInput;
  }
  expect(refused, 'malformed transaction refused');
  for (final n in nodes) {
    n.free();
  }
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
        a, bc.initiateSecureSession(b.kem.public, a.sig).handshakeMessage);
  } on AviaryException catch (e) {
    wrong = e.status == AviaryStatus.badInput;
  }
  expect(wrong, 'wrong responder identity refused');
  init.free();
  resp.free();
}
