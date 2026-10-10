import 'dart:io';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void check(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

void main() {
  test('persistence', () async {
    final bc = Aviary.instance;
    final node = bc.createNode(77);
    final alice = node.fundAccount(500);
    final bob = node.fundAccount(0);
    node.sendTo(alice, bob, 120);
    node.commit();
    node.sendTo(alice, bob, 30);
    node.commit();
    node.sendTo(alice, bob, 1);
    check(node.mempoolSize() == 1, 'one pending transfer');

    final snap = node.snapshot();
    final back = bc.restoreNode(snap);
    check(back.height() == 2, 'height restored');
    check(back.balance(bob.address) == 150, 'balances restored');
    check(back.nonce(alice.address) == 2, 'nonce restored');
    check(back.stateRoot().equalsBytes(node.stateRoot()), 'state root equal');
    check(back.mempoolSize() == 0, 'pending transfers are not persisted');
    check(back.blockAt(1).equalsBytes(node.blockAt(1)), 'archive restored');
    check(back.events().isEmpty, 'replay leaves no events');
    back.sendTo(alice, bob, 1);
    check(back.commit() == 3, 'restored node keeps producing');
    check(back.balance(bob.address) == 151, 'new block applied');

    final dir = await Directory.systemTemp.createTemp('aviary_persist');
    final path = '${dir.path}/chain.bin';
    check(await bc.restoreNodeFrom(path) == null, 'missing file is null');
    await back.saveTo(path);
    check(!File('$path.tmp').existsSync(), 'temp file renamed away');
    final again = await bc.restoreNodeFrom(path);
    check(again != null && again.height() == 3, 'file round trip');
    await dir.delete(recursive: true);

    var status = AviaryStatus.ok;
    try {
      bc.restoreNode(Uint8List.fromList([1, 2, 3]));
    } on AviaryException catch (e) {
      status = e.status;
    }
    check(status == AviaryStatus.badInput, 'garbage is badInput');
    var absent = false;
    try {
      node.blockAt(9);
    } on AviaryException catch (_) {
      absent = true;
    }
    check(absent, 'missing block throws');
    final fresh = bc.restoreNode(bc.createNode(78).snapshot());
    check(fresh.height() == 0, 'empty snapshot restores a fresh node');
    for (final n in [node, back, again!, fresh]) {
      n.dispose();
    }
    print('aviary dart persistence smoke: OK');
  });
}
