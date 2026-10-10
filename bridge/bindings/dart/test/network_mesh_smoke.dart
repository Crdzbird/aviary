import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'support/check.dart';

import 'support/mesh.dart';

void main() {
  mesh(Aviary.instance);
  print('aviary dart network mesh smoke: OK');
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
  expect(
    nodes[0].onGossip(1, Uint8List.fromList([9, 9, 9])).relays.isEmpty,
    'garbage relays nothing',
  );
  expect(!nodes[0].peerBanned(1), 'one bad payload does not ban');
  expect(
    bc.verifyStateProof(nodes[2].stateRoot(), nodes[2].proveAccount(alice)),
    'proof from a networked node verifies',
  );
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
      Uint8List.fromList([1, 2]),
    ]);
  } on AviaryException catch (e) {
    refused = e.status == AviaryStatus.badInput;
  }
  expect(refused, 'malformed transaction refused');
  for (final n in nodes) {
    n.free();
  }
}
