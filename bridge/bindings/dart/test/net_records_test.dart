import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('records on a networked node', () {
    netRecords(Aviary.instance);
    print('aviary dart networked records smoke: OK');
  });
}

void netRecords(Aviary bc) {
  final keys = List.generate(4, (_) => bc.newValidator());
  final genesis = bc.buildGenesis(
    chainId: 77,
    accounts: const [],
    validators: [for (final k in keys) k.entry(1)],
    stateModel: 2,
  );
  final net = PeerMesh([
    for (var i = 0; i < 4; i++)
      bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
    bc.newNetNode(genesis: genesis, me: AviaryNetwork.observer),
  ])..linkAll();
  final nodes = net.nodes;
  final author = bc.createNode(1).newAccount();
  final heir = bc.createNode(2).newAccount();
  final key = Uint8List(32)..fillRange(0, 32, 0x42);
  Uint8List tx(int nonce, RecordOp op, Uint8List value) => bc.signRecordTx(
    chainId: 77,
    nonce: nonce,
    key: key,
    op: op,
    value: value,
    secret: author.secret,
  );
  void give(List<Uint8List> txs) {
    for (final n in nodes.take(4)) {
      n.setTransactions(txs);
    }
  }

  check(nodes[0].recordNonce(author.address) == 0, 'fresh author nonce 0');
  give([
    tx(0, RecordOp.put, Uint8List.fromList([1, 2, 3])),
  ]);
  net.start();
  check(net.settle(validators: [0, 1, 2, 3]), 'height 1 commits');
  for (final n in nodes) {
    final rec = n.record(key);
    check(rec != null && rec.version == 1, 'every node holds version 1');
    check(rec!.owner.equalsBytes(author.address), 'author owns the record');
  }
  check(nodes[4].recordNonce(author.address) == 1, 'observer sees nonce 1');
  final proof = nodes[1].proveRecord(key);
  check(
    bc.verifyStateProof(nodes[4].stateRoot(), proof),
    'a validator proof verifies under the observer root',
  );
  check(
    bc.recordFromProof(proof).value.equalsBytes(Uint8List.fromList([1, 2, 3])),
    'the proof carries the value',
  );
  check(nodes[2].record(Uint8List(32)) == null, 'unknown key reads null');

  give([tx(1, RecordOp.transfer, heir.address)]);
  net.advance();
  check(net.settle(validators: [0, 1, 2, 3]), 'height 2 commits');
  check(
    nodes.every((n) => n.record(key)!.owner.equalsBytes(heir.address)),
    'a list set after the commit lands in the next height',
  );
  net.advance();
  check(net.settle(validators: [0, 1, 2, 3]), 'height 3 commits empty');
  check(nodes[3].record(key)!.version == 2, 'the spent list is not replayed');
  net.free();

  final account = bc.newNetNode(
    genesis: bc.buildGenesis(
      chainId: 78,
      accounts: const [],
      validators: [keys[0].entry(1)],
    ),
    me: 0,
    blsKey: keys[0],
  );
  var refused = false;
  try {
    account.record(key);
  } on AviaryException catch (e) {
    refused = e.message.contains('not a record-model chain');
  }
  check(refused, 'an account network refuses record queries');
  account.free();
}
