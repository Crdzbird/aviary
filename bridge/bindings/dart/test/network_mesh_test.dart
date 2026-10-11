import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('network mesh', () {
    mesh(Aviary.instance);
    print('aviary dart network mesh smoke: OK');
  });
}

void mesh(Aviary bc) {
  final keys = List.generate(4, (_) => bc.newValidator());
  final alice = Uint8List(32)..fillRange(0, 32, 0x11);
  final genesis = bc.buildGenesis(
    chainId: 42,
    accounts: [(address: alice, balance: 1000)],
    validators: [for (final k in keys) k.entry(1)],
  );
  final net = PeerMesh([
    for (var i = 0; i < 4; i++)
      bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
    bc.newNetNode(genesis: genesis, me: AviaryNetwork.observer),
  ]);
  for (var i = 0; i < 4; i++) {
    net.link(i, i + 1);
  }
  final nodes = net.nodes;
  const validators = [0, 1, 2, 3];
  net.start();
  check(net.settle(validators: validators), 'line mesh commits via relays');
  check(!net.deliver(), 'settle drains the relays');
  check(nodes.every((n) => n.height == 1), 'every node incl. observer at 1');
  check(nodes[0].tip().equalsBytes(nodes[4].tip()), 'observer has the tip');
  check(nodes[3].balance(alice) == 1000, 'genesis balance visible');
  check(nodes[0].replayLog().isNotEmpty, 'catch-up log holds the height');
  check(
    nodes[0].onGossip(1, Uint8List.fromList([9, 9, 9])).relays.isEmpty,
    'garbage relays nothing',
  );
  check(!nodes[0].peerBanned(1), 'one bad payload does not ban');
  check(
    bc.verifyStateProof(nodes[2].stateRoot(), nodes[2].proveAccount(alice)),
    'proof from a networked node verifies',
  );
  net.advance();
  check(nodes[3].replayLog().isEmpty, 'advance restarts a follower log');
  check(nodes[0].replayLog().isNotEmpty, 'proposer logs its new proposal');
  check(!nodes[0].committed, 'new height is open');
  check(net.settle(validators: validators), 'second height commits');
  check(nodes.every((n) => n.height == 2), 'second height committed');
  var refused = false;
  try {
    nodes[0].setTransactions([
      Uint8List.fromList([1, 2]),
    ]);
  } on AviaryException catch (e) {
    refused = e.status == AviaryStatus.badInput;
  }
  check(refused, 'malformed transaction refused');
  final full = PeerMesh([
    for (var i = 0; i < 4; i++)
      bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
  ])..linkAll();
  full.start();
  check(full.settle() && full.nodes.every((n) => n.height == 1), 'full mesh');
  check(!full.run(() => false, budget: 1), 'run reports a miss');
  full.free();
  net.free();
}
