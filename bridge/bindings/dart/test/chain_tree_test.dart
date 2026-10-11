import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/tree_fixture.dart';

void main() {
  test('chain tree', () {
    final bc = Aviary.instance;
    final fx = TreeFixture(bc);
    final tree = bc.buildChainTree(fx.node());
    check(tree.chainIds().join(',') == '1,42,420,43', 'pre-order ids');
    final finalized = <int, int>{};
    tree.onFinalized = (id, _) => finalized[id] = (finalized[id] ?? 0) + 1;
    tree.produce(420, 2);
    tree.produce(420, 3);
    check(
      tree.pendingAnchors(42) == 1 && tree.pendingAnchors(1) == 0,
      'pending anchors',
    );
    tree.produce(42, 2);
    tree.produce(43, 2);
    final rootBlk = tree.produce(1, 2);
    check(
      tree.height(1) == 1 &&
          tree.height(42) == 1 &&
          tree.height(420) == 2 &&
          tree.pendingAnchors(1) == 0,
      'heights',
    );
    check(
      finalized[420] == 2 &&
          finalized[42] == 1 &&
          finalized[1] == 1 &&
          finalized[43] == 1,
      'finalized hook from Dart',
    );
    check(failsAnyhow(() => tree.produce(7, 3)), 'unknown chain');
    final proof = tree.proveAccount(420, fx.alice);
    check(
      tree.verify(proof) &&
          bc.verifySubchainProof(
            bc.headerSubchainRoot(bc.blockHeader(rootBlk)),
            proof,
          ),
      'grandchild proven from the root',
    );
    check(!tree.verify(tampered(proof)), 'tampered proof fails');
    check(
      bc.genesisHeader(tree.genesis(42)).isNotEmpty && tree.tip(420).isNotEmpty,
      'genesis/tip',
    );
    var calls = 0;
    tree.txSource = (_, __) {
      calls++;
      return [];
    };
    tree.produce(420, 4);
    check(calls == 1, 'tx source from Dart');
    tree.free();
    print('aviary dart chain-tree smoke: OK');
  });
}
