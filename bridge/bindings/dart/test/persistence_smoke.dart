import 'dart:io';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Future<void> main() async {
  final bc = Aviary.instance;
  final node = bc.createNode(77);
  final alice = node.fundAccount(500);
  final bob = node.fundAccount(0);
  node.sendTo(alice, bob, 120);
  node.commit();
  node.sendTo(alice, bob, 30);
  node.commit();
  node.sendTo(alice, bob, 1);
  expect(node.mempoolSize() == 1, 'one pending transfer');

  final snap = node.snapshot();
  final back = bc.restoreNode(snap);
  expect(back.height() == 2, 'height restored');
  expect(back.balance(bob.address) == 150, 'balances restored');
  expect(back.nonce(alice.address) == 2, 'nonce restored');
  expect(back.stateRoot().equalsBytes(node.stateRoot()), 'state root equal');
  expect(back.mempoolSize() == 0, 'pending transfers are not persisted');
  expect(back.blockAt(1).equalsBytes(node.blockAt(1)), 'archive restored');
  expect(back.events().isEmpty, 'replay leaves no events');
  back.sendTo(alice, bob, 1);
  expect(back.commit() == 3, 'restored node keeps producing');
  expect(back.balance(bob.address) == 151, 'new block applied');

  final dir = await Directory.systemTemp.createTemp('aviary_persist');
  final path = '${dir.path}/chain.bin';
  expect(await bc.restoreNodeFrom(path) == null, 'missing file is null');
  await back.saveTo(path);
  expect(!File('$path.tmp').existsSync(), 'temp file renamed away');
  final again = await bc.restoreNodeFrom(path);
  expect(again != null && again.height() == 3, 'file round trip');
  await dir.delete(recursive: true);

  var status = AviaryStatus.ok;
  try {
    bc.restoreNode(Uint8List.fromList([1, 2, 3]));
  } on AviaryException catch (e) {
    status = e.status;
  }
  expect(status == AviaryStatus.badInput, 'garbage is badInput');
  var absent = false;
  try {
    node.blockAt(9);
  } on AviaryException catch (_) {
    absent = true;
  }
  expect(absent, 'missing block throws');
  final fresh = bc.restoreNode(bc.createNode(78).snapshot());
  expect(fresh.height() == 0, 'empty snapshot restores a fresh node');
  for (final n in [node, back, again!, fresh]) {
    n.dispose();
  }
  print('aviary dart persistence smoke: OK');
}
