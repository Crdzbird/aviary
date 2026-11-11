import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/dynamic_fixture.dart';

void main() {
  test('dynamic subchain: refusals name their reason', () {
    final bc = Aviary.instance;
    final accounts = bc.createNode(0x5502);
    final secret = accounts.newAccount().secret;
    final stranger = accounts.newAccount().secret;
    final fx = DynamicFixture(bc, secret, serverSeed: 0x03, userSeed: 0x13);
    final mainGenesis = fx.parentGenesis(8);
    final mainHash = bc.genesisHash(mainGenesis);
    final userGenesis = fx.childGenesis(8001, 8, mainHash);
    final main = fx.assemble(mainGenesis, fx.server);
    final op = bc.opRegister(fx.registration(8001, userGenesis));

    String refusal(Uint8List operation) {
      try {
        main.proposeSubchain(10, ops: [operation]);
      } on AviaryException catch (e) {
        return e.message;
      }
      return '';
    }

    Uint8List signedBy(Uint8List key, Uint8List parentHash, [int parent = 8]) =>
        bc.signOp(
          op,
          parentChainId: parent,
          parentGenesisHash: parentHash,
          registrarIndex: 0,
          secret: key,
        );

    check(refusal(op).contains('not enough approvals'), 'unsigned');
    check(
      refusal(signedBy(stranger, mainHash)).contains('bad registrar signature'),
      'a stranger cannot register',
    );
    check(
      refusal(signedBy(secret, tampered(mainHash))).contains('signature'),
      'another parent genesis',
    );
    check(
      refusal(signedBy(secret, mainHash, 9)).contains('signature'),
      'another parent id',
    );
    final good = signedBy(secret, mainHash);
    check(
      fails(
        () => bc.signOp(
          good,
          parentChainId: 8,
          parentGenesisHash: mainHash,
          registrarIndex: 0,
          secret: secret,
        ),
      ),
      'a registrar approves once',
    );
    fx.commit(main, main.proposeSubchain(10, ops: [good]));
    check(refusal(good).contains('already registered'), 'an id is used once');
    final record = main.childRecord(8001);
    final wrongTip = bc.opClose(
      chainId: 8001,
      finalHeight: record.tipHeight,
      finalTipHash: fill(0x09),
    );
    check(
      refusal(fx.sign(wrongTip, 8, mainHash)).contains('recorded tip'),
      'a close must name the recorded tip',
    );
    final snapshot = main.snapshot();
    final trailing = Uint8List.fromList([...snapshot, 0]);
    check(
      fails(
        () => bc.restoreChain(
          mainGenesis,
          0,
          fx.server.public,
          fx.server.secret,
          trailing,
        ),
      ),
      'trailing bytes are refused',
    );
    check(
      bc.chainSnapshotError(userGenesis, snapshot).contains('another genesis'),
      'a snapshot is bound to its genesis',
    );
    main.free();
    accounts.dispose();
  });
}
