import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('chain extensions', () {
    final bc = Aviary.instance;
    chainShortcuts(bc);
    thresholdShortcut(bc);
    treeShortcuts(bc);
    print('aviary dart chain extensions smoke: OK');
  });
}

void chainShortcuts(Aviary bc) {
  final validator = bc.newValidator();
  check(validator.public.length == 48, 'fresh BLS validator');
  check(validator.entry(3).power == 3, 'validator entry');
  check(
    validator.treeValidator(1, holdsSecret: false).secret == null,
    'tree validator without secret',
  );
  final bft = bc.assembleSingleValidator(chainId: 9, validator: validator);
  final block = bft.produceBlock(2);
  check(block.isNotEmpty && bft.height == 1, 'bft block produced');
  bft.free();
  final pos = bc.assembleSingleValidator(
    chainId: 10,
    validator: validator,
    blueprint: bc.makeBlueprint(
      consensus: 'pos',
      stateModel: 'account',
      proposer: 'round-robin',
    ),
  );
  pos.produceBlock(2, justify: pos.seal);
  check(pos.height == 1, 'pos block produced');
  pos.free();
}

void thresholdShortcut(Aviary bc) {
  final shares = bc.createThresholdAccount(3, 2);
  final message = 'pay'.utf8Bytes;
  final sig = bc.signWithShares([shares[0], shares[2]], message);
  check(sig.length == 64, 'threshold signature aggregated');
  check(
    failsAnyhow(() => bc.signWithShares([], message)),
    'empty shares refused',
  );
}

void treeShortcuts(Aviary bc) {
  final v = bc.newValidator().treeValidator(1);
  final root = const TreeNode(chainId: 1, validators: [])
      .copyWith(validators: [v])
      .withChild(TreeNode(chainId: 2, validators: [v]))
      .withChild(TreeNode(chainId: 3, validators: [v]));
  check(root.chainIds.join(',') == '1,2,3', 'tree chain ids');
  check(root.find(3)?.chainId == 3 && root.find(4) == null, 'tree find');
  final tree = bc.buildChainTree(root);
  final blocks = tree.produceRound(5);
  check(blocks.length == 3 && tree.height(1) == 1, 'round produced');
  check(tree.pendingAnchors(1) == 0, 'round anchored children');
  tree.free();
}
