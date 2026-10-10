import 'dart:async';

import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Future<void> main() async {
  final bc = Aviary.instance;
  pulled(bc);
  await pushed(bc);
  decoding();
  print('aviary dart events smoke: OK');
}

void pulled(Aviary bc) {
  final node = bc.createNode(1);
  final alice = node.fundAccount(100);
  final bob = node.fundAccount(0);
  final txid = node.sendTo(alice, bob, 5);
  node.commit();
  final events = node.events();
  final admitted = events.whereType<TxAdmitted>().single;
  expect(admitted.txid.equalsBytes(txid), 'admitted txid matches');
  final removed = events.whereType<TxRemoved>().single;
  expect(removed.txid.equalsBytes(txid), 'removed txid matches');
  final finalized = events.whereType<BlockFinalized>().single;
  expect(finalized.height == 1 && finalized.txCount == 1, 'block fields');
  expect(finalized.blockId.length == 32, 'block id present');
  expect(finalized.stateRoot.equalsBytes(node.stateRoot()), 'state root');
  expect(node.events().isEmpty, 'drain empties the buffer');
  node.dispose();
}

Future<void> pushed(Aviary bc) async {
  final node = bc.createNode(2);
  final seen = <ChainEvent>[];
  var done = false;
  final sub = node.eventStream.listen(seen.add, onDone: () => done = true);
  final alice = node.fundAccount(100);
  node.sendTo(alice, node.fundAccount(0), 7);
  node.commit();
  expect(seen.isEmpty, 'listeners run after the native call returns');
  await Future<void>.delayed(Duration.zero);
  expect(seen.length == 3, 'three events pushed');
  expect(seen.first is TxAdmitted && seen.last is TxRemoved, 'order');
  expect(node.events().isEmpty, 'buffer stays empty while streaming');
  final labels = [
    for (final e in seen)
      switch (e) {
        BlockFinalized(:final height) => 'block $height',
        BlockRejected(:final attemptedHeight) => 'rejected $attemptedHeight',
        TxAdmitted(:final mempoolSize) => 'admitted $mempoolSize',
        TxRemoved(:final mempoolSize) => 'removed $mempoolSize',
        UnknownEvent(:final type) => 'unknown $type',
      },
  ];
  expect(labels.join(',') == 'admitted 1,block 1,removed 0', 'exhaustive');
  node.dispose();
  await Future<void>.delayed(Duration.zero);
  expect(done, 'dispose closes the stream');
  await sub.cancel();
}

void decoding() {
  final unknown = ChainEvent.fromJson({'type': 'reorg', 'depth': 2});
  expect(unknown is UnknownEvent && unknown.json['depth'] == 2, 'unknown kept');
  final typeless = ChainEvent.fromJson({'height': 1});
  expect(typeless is UnknownEvent && typeless.type.isEmpty, 'typeless');
  final partial = ChainEvent.fromJson({'type': 'block_finalized'});
  expect(partial is BlockFinalized && partial.blockId.isEmpty, 'lenient');
}
