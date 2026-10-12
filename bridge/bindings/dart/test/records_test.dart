import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('records', () {
    final bc = Aviary.instance;
    final chain = bc.assembleRecordChain(
      chainId: 9,
      validator: bc.newValidator(),
    );
    check(chain.chainId == 9, 'chain id');
    final alice = bc.createNode(1).newAccount();
    final bob = bc.createNode(2).newAccount();
    final key = bc.hash(utf8.encode('part:ABC-123'));
    Uint8List json(Map<String, Object> m) => utf8.encode(jsonEncode(m));
    chain.write(
      alice.secret,
      key,
      RecordOp.put,
      json({'serial': 'ABC-123', 'status': 'built'}),
    );
    chain.write(
      alice.secret,
      key,
      RecordOp.put,
      json({'serial': 'ABC-123', 'status': 'shipped'}),
    );
    check(chain.pendingCount == 2, 'two writes queued with consecutive nonces');
    check(chain.record(key) == null, 'nothing committed before a block');
    check(
      fails(() => chain.write(bob.secret, key, RecordOp.put, json({'x': 1}))),
      'non-owner refused at queue time',
    );
    chain.produceBlock(2);
    check(
      chain.pendingCount == 0 && chain.height == 1,
      'block drained the queue',
    );
    final rec = chain.record(key)!;
    check(
      rec.version == 2 && rec.owner.equalsBytes(alice.address),
      'owner and version',
    );
    check(
      jsonDecode(utf8.decode(rec.value))['status'] == 'shipped',
      'latest value',
    );
    final proof = chain.proveRecord(key);
    check(
      bc.verifyStateProof(chain.stateRoot(), proof),
      'proof verifies against the root',
    );
    final leaf = bc.recordFromProof(proof);
    check(
      leaf.key.equalsBytes(key) &&
          leaf.version == 2 &&
          leaf.value.equalsBytes(rec.value),
      'proof carries the record',
    );
    check(
      !bc.verifyStateProof(
        chain.stateRoot(),
        Uint8List.fromList(proof)..[proof.length - 1] ^= 1,
      ),
      'tampered proof fails',
    );
    chain.write(alice.secret, key, RecordOp.transfer, bob.address);
    chain.produceBlock(3);
    check(chain.record(key)!.owner.equalsBytes(bob.address), 'ownership moved');
    check(
      fails(() => chain.write(alice.secret, key, RecordOp.put, json({'x': 1}))),
      'former owner refused',
    );
    chain.write(bob.secret, key, RecordOp.put, json({'status': 'installed'}));
    chain.write(bob.secret, key, RecordOp.delete, Uint8List(0));
    chain.produceBlock(4);
    check(chain.record(key) == null, 'deleted');
    check(
      chain.events().whereType<BlockFinalized>().length == 3,
      'three finalized blocks reported',
    );
    chain.free();
    print('aviary dart records smoke: OK');
  });
}
