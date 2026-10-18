import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('records from a key store and hybrid keys', () {
    final bc = Aviary.instance;
    final chain = bc.assembleRecordChain(
      chainId: 41,
      validator: bc.newValidator(),
    );
    final node = bc.createNode(1);
    final ed = node.newAccount();
    final hy = node.newAccountHybrid();
    final store = bc.createKeyStore('pass');
    final edAddr = store.addAccount(ed.publicKey, ed.secret);
    final hyAddr = store.addAccount(hy.publicKey, hy.secret);
    final k1 = bc.hash(utf8.encode('asset:1'));
    final k2 = bc.hash(utf8.encode('asset:2'));
    final k3 = bc.hash(utf8.encode('asset:3'));
    final seen = <ChainEvent>[];
    chain.writeFromStore(store, edAddr, k1, RecordOp.put, utf8.encode('a'));
    chain.writeFromStore(store, hyAddr, k2, RecordOp.put, utf8.encode('b'));
    chain.addTx(
      bc.signRecordTxHybrid(
        chainId: 41,
        nonce: chain.recordNonce(hy.address),
        key: k3,
        op: RecordOp.put,
        value: utf8.encode('c'),
        publicKey: hy.publicKey,
        secret: hy.secret,
      ),
    );
    seen.addAll(chain.events());
    check(
      seen.whereType<TxAdmitted>().length == 3,
      'each queued write is an admission event',
    );
    chain.produceBlock(2);
    final after = chain.events();
    check(
      after.whereType<TxRemoved>().length == 3 &&
          after.whereType<BlockFinalized>().length == 1,
      'the block removes the three and finalizes',
    );
    check(chain.record(k1)!.owner.equalsBytes(edAddr), 'ed25519 author');
    check(chain.record(k2)!.owner.equalsBytes(hyAddr), 'hybrid author');
    check(chain.record(k3)!.owner.equalsBytes(hy.address), 'raw hybrid');
    check(chain.recordNonce(hyAddr) == 2, 'hybrid nonce advanced twice');
    check(chain.metricValues()['aviary_mempool_size'] == 0, 'queue drained');
    store.lock();
    check(
      failsAnyhow(
        () => chain.writeFromStore(
          store,
          edAddr,
          k1,
          RecordOp.delete,
          Uint8List(0),
        ),
      ),
      'a locked store cannot sign',
    );
    store.dispose();
    node.dispose();
    chain.free();
    print('aviary dart records from a key store smoke: OK');
  });
}
