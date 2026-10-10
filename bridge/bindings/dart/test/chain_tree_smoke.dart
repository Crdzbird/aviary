import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool throws(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}

Uint8List fill(int b) => Uint8List(32)..fillRange(0, 32, b);

void main() {
  final bc = Aviary.instance;
  ({Uint8List publicKey, Uint8List? secret, int power}) val(int seed) {
    final v = bc.newBLSValidator(fill(seed));
    return (publicKey: v.public, secret: v.secret, power: 1);
  }

  final alice = fill(0xA1);
  final pos = bc.makeBlueprint(
      consensus: 'pos', stateModel: 'account', proposer: 'round-robin');
  TreeNode mk() => TreeNode(chainId: 1, blueprint: pos, validators: [
        val(0x01)
      ], children: [
        TreeNode(
            chainId: 42,
            validators: [val(0x10), val(0x11), val(0x12)],
            maxHeadersPerAnchor: 4,
            children: [
              TreeNode(
                  chainId: 420,
                  validators: [val(0x20), val(0x21)],
                  accounts: [(address: alice, balance: 1000)])
            ]),
        TreeNode(chainId: 43, validators: [val(0x30)])
      ]);
  final base = mk();
  expect(
      throws(() => bc.buildChainTree(base.copyWith(children: [
            base.children[0],
            base.children[1].copyWith(chainId: 42)
          ]))),
      'duplicate id refused');
  expect(
      throws(() => bc.buildChainTree(base.copyWith(children: [
            base.children[0].copyWith(blueprint: pos),
            base.children[1]
          ]))),
      'PoS child refused');
  expect(
      throws(() => bc.buildChainTree(base.copyWith(
          blueprint: bc.makeBlueprint(
              consensus: 'no-such-engine', stateModel: 'account')))),
      'unknown engine refused');
  final tree = bc.buildChainTree(mk());
  expect(tree.chainIds().join(',') == '1,42,420,43', 'pre-order ids');
  final finalized = <int, int>{};
  tree.onFinalized = (id, _) => finalized[id] = (finalized[id] ?? 0) + 1;
  tree.produce(420, 2);
  tree.produce(420, 3);
  expect(tree.pendingAnchors(42) == 1 && tree.pendingAnchors(1) == 0,
      'pending anchors');
  tree.produce(42, 2);
  tree.produce(43, 2);
  final rootBlk = tree.produce(1, 2);
  expect(
      tree.height(1) == 1 &&
          tree.height(42) == 1 &&
          tree.height(420) == 2 &&
          tree.pendingAnchors(1) == 0,
      'heights');
  expect(
      finalized[420] == 2 &&
          finalized[42] == 1 &&
          finalized[1] == 1 &&
          finalized[43] == 1,
      'finalized hook from Dart');
  expect(throws(() => tree.produce(7, 3)), 'unknown chain');
  final proof = tree.proveAccount(420, alice);
  expect(
      tree.verify(proof) &&
          bc.verifySubchainProof(
              bc.headerSubchainRoot(bc.blockHeader(rootBlk)), proof),
      'grandchild proven from the root');
  final bad = Uint8List.fromList(proof)..[proof.length - 1] ^= 1;
  expect(!tree.verify(bad), 'tampered proof fails');
  expect(
      bc.genesisHeader(tree.genesis(42)).isNotEmpty && tree.tip(420).isNotEmpty,
      'genesis/tip');
  var calls = 0;
  tree.txSource = (_, __) {
    calls++;
    return [];
  };
  tree.produce(420, 4);
  expect(calls == 1, 'tx source from Dart');
  tree.free();
  print(
      'aviary dart chain-tree smoke: OK (root <- child <- grandchild built together, produced upward, proven from the root, hooks from Dart)');
}
