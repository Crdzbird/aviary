import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'support/check.dart';

void main() {
  final bc = Aviary.instance;
  hexHelpers();
  nodeShortcuts(bc);
  print('aviary dart extensions smoke: OK');
}

void hexHelpers() {
  final bytes = Uint8List.fromList([0, 1, 0xab, 0xff]);
  expect(bytes.hex == '0001abff', 'hex renders');
  expect('0x0001ABFF'.hexBytes.equalsBytes(bytes), 'hex parses with prefix');
  expect(failsAnyhow(() => 'abc'.hexBytes), 'odd hex length refused');
  expect(failsAnyhow(() => 'zz'.hexBytes), 'non hex refused');
  expect(Uint8List(12).shortHex == '00000000..00000000', 'short hex');
  expect('hi'.utf8Bytes.length == 2, 'utf8 bytes');
}

void nodeShortcuts(Aviary bc) {
  final node = bc.createNode(7);
  final alice = node.fundAccount(1000);
  final bob = node.fundAccount(0);
  final carol = node.fundHybridAccount(50);
  expect(!alice.isHybrid && carol.isHybrid, 'scheme detected from secret');
  expect(alice.addressHex.length == 64, 'address hex');
  node.sendTo(alice, bob, 250);
  node.send(carol, bob.address, 10);
  expect(
    failsAnyhow(() => node.send(carol, bob.address, 1, fee: 1)),
    'hybrid fee refused',
  );
  expect(node.commit() == 1, 'first block committed');
  node.sendTo(alice, bob, 5, fee: 1);
  expect(node.commit(timestamp: 99) == 2, 'second block committed');
  expect(node.balance(bob.address) == 265, 'balances moved');
  expect(
    node.verifyAccountProof(node.proveAccount(bob.address)),
    'account proof verifies',
  );
  expect(node.events().isNotEmpty, 'events decoded');
  expect(node.metricValues().isNotEmpty, 'metrics decoded');
  node.dispose();
}
