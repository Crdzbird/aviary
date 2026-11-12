import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/dynamic_fixture.dart';

void main() {
  test('dynamic subchain: register, anchor, restart, prove', () {
    final bc = Aviary.instance;
    final secret = bc.createNode(0x5501).newAccount().secret;
    final fx = DynamicFixture(bc, secret, serverSeed: 0x02, userSeed: 0x12);
    final mainGenesis = fx.parentGenesis(7);
    final mainHash = bc.genesisHash(mainGenesis);
    final userGenesis = fx.childGenesis(7001, 7, mainHash);
    final main = fx.assemble(mainGenesis, fx.server);
    final user = fx.assemble(userGenesis, fx.user);
    check(main.registryCounts()?.records == 0, 'empty dynamic registry');
    check(user.registryCounts() == null, 'a leaf chain has no registry');

    final op = fx.sign(
      bc.opRegister(fx.registration(7001, userGenesis)),
      7,
      mainHash,
    );
    fx.commit(main, main.proposeSubchain(10, ops: [op]));
    fx.commit(user, user.propose(20));
    final anchor = bc.pendingAnchor(main, user)!;
    fx.commit(main, main.proposeSubchain(11, anchors: [anchor]));
    check(bc.pendingAnchor(main, user) == null, 'nothing pending');

    final mainSnap = main.snapshot();
    check(
      bc.chainSnapshotError(mainGenesis, mainSnap).isEmpty,
      'good snapshot',
    );
    final mainBack = bc.restoreChain(
      mainGenesis,
      0,
      fx.server.public,
      fx.server.secret,
      mainSnap,
    );
    final userBack = bc.restoreChain(
      userGenesis,
      0,
      fx.user.public,
      fx.user.secret,
      user.snapshot(),
    );
    check(mainBack.snapshot().equalsBytes(mainSnap), 'snapshot round trips');
    check(mainBack.tip().equalsBytes(main.tip()), 'tip restored');
    for (final u in [user, userBack]) {
      fx.commit(u, u.propose(21));
    }
    final straight = fx.commit(
      main,
      main.proposeSubchain(12, anchors: [bc.pendingAnchor(main, user)!]),
    );
    final resumed = fx.commit(
      mainBack,
      mainBack.proposeSubchain(
        12,
        anchors: [bc.pendingAnchor(mainBack, userBack)!],
      ),
    );
    check(straight.equalsBytes(resumed), 'a restored chain continues equally');
    check(
      mainBack.snapshot().equalsBytes(main.snapshot()),
      'and stays byte for byte the same',
    );
    final range = mainBack.archiveRange()!;
    check(range.first == 1 && range.last == 3, 'archive restored');
    check(mainBack.archivedBlock(3).equalsBytes(straight), 'archived block');
    check(mainBack.archivedCertificate(3).isNotEmpty, 'archived certificate');
    check(mainBack.pruneArchive(3) == 2, 'prune');

    final record = mainBack.childRecord(7001);
    final root = bc.headerSubchainRoot(mainBack.tip());
    final proof = mainBack.childProof(7001);
    final proven = bc.verifyChildProof(root, proof);
    check(
      proven.tipHeight == 2 &&
          proven.status == ChildStatus.open &&
          proven.tipHash.equalsBytes(bc.encodedHeaderHash(userBack.tip())) &&
          proven.encoded.equalsBytes(record.encoded),
      'the proven record is the anchored user tip',
    );
    check(fails(() => bc.verifyChildProof(root, tampered(proof))), 'tampered');
    check(fails(() => bc.verifyChildProof(fill(1), proof)), 'wrong root');
    for (final c in [main, user, mainBack, userBack]) {
      c.free();
    }
  });
}
