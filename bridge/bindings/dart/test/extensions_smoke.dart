import 'dart:typed_data';

import 'package:blockchainer_dart/blockchainer.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool fails(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}

void main() {
  final bc = Blockchainer.instance;
  hexHelpers();
  nodeShortcuts(bc);
  chainShortcuts(bc);
  thresholdShortcut(bc);
  treeShortcuts(bc);
  print('blockchainer dart extensions smoke: OK');
}

void hexHelpers() {
  final bytes = Uint8List.fromList([0, 1, 0xab, 0xff]);
  expect(bytes.hex == '0001abff', 'hex renders');
  expect('0x0001ABFF'.hexBytes.equalsBytes(bytes), 'hex parses with prefix');
  expect(fails(() => 'abc'.hexBytes), 'odd hex length refused');
  expect(fails(() => 'zz'.hexBytes), 'non hex refused');
  expect(Uint8List(12).shortHex == '00000000..00000000', 'short hex');
  expect('hi'.utf8Bytes.length == 2, 'utf8 bytes');
}

void nodeShortcuts(Blockchainer bc) {
  final node = bc.createNode(7);
  final alice = node.fundAccount(1000);
  final bob = node.fundAccount(0);
  final carol = node.fundHybridAccount(50);
  expect(!alice.isHybrid && carol.isHybrid, 'scheme detected from secret');
  expect(alice.addressHex.length == 64, 'address hex');
  node.sendTo(alice, bob, 250);
  node.send(carol, bob.address, 10);
  expect(fails(() => node.send(carol, bob.address, 1, fee: 1)),
      'hybrid fee refused');
  expect(node.commit() == 1, 'first block committed');
  node.sendTo(alice, bob, 5, fee: 1);
  expect(node.commit(timestamp: 99) == 2, 'second block committed');
  expect(node.balance(bob.address) == 265, 'balances moved');
  expect(node.verifyAccountProof(node.proveAccount(bob.address)),
      'account proof verifies');
  expect(node.events().isNotEmpty, 'events decoded');
  expect(node.metricValues().isNotEmpty, 'metrics decoded');
  node.dispose();
}

void chainShortcuts(Blockchainer bc) {
  final validator = bc.newValidator();
  expect(validator.public.length == 48, 'fresh BLS validator');
  expect(validator.entry(3).power == 3, 'validator entry');
  expect(validator.treeValidator(1, holdsSecret: false).secret == null,
      'tree validator without secret');
  final bft = bc.assembleSingleValidator(chainId: 9, validator: validator);
  final block = bft.produceBlock(2);
  expect(block.isNotEmpty && bft.height == 1, 'bft block produced');
  bft.free();
  final pos = bc.assembleSingleValidator(
    chainId: 10,
    validator: validator,
    blueprint: bc.makeBlueprint(
        consensus: 'pos', stateModel: 'account', proposer: 'round-robin'),
  );
  pos.produceBlock(2, justify: pos.seal);
  expect(pos.height == 1, 'pos block produced');
  pos.free();
}

void thresholdShortcut(Blockchainer bc) {
  final shares = bc.createThresholdAccount(3, 2);
  final message = 'pay'.utf8Bytes;
  final sig = bc.signWithShares([shares[0], shares[2]], message);
  expect(sig.length == 64, 'threshold signature aggregated');
  expect(fails(() => bc.signWithShares([], message)), 'empty shares refused');
}

void treeShortcuts(Blockchainer bc) {
  final v = bc.newValidator().treeValidator(1);
  final root = const TreeNode(chainId: 1, validators: [])
      .copyWith(validators: [v])
      .withChild(TreeNode(chainId: 2, validators: [v]))
      .withChild(TreeNode(chainId: 3, validators: [v]));
  expect(root.chainIds.join(',') == '1,2,3', 'tree chain ids');
  expect(root.find(3)?.chainId == 3 && root.find(4) == null, 'tree find');
  final tree = bc.buildChainTree(root);
  final blocks = tree.produceRound(5);
  expect(blocks.length == 3 && tree.height(1) == 1, 'round produced');
  expect(tree.pendingAnchors(1) == 0, 'round anchored children');
  tree.free();
}
