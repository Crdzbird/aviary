import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';
import 'support/tree_fixture.dart';

void main() {
  test('chain tree snapshot and restore', () {
    final bc = Aviary.instance;
    final fx = TreeFixture(bc);
    final straight = bc.buildChainTree(fx.node());
    final first = bc.buildChainTree(fx.node());
    Uint8List round(ChainTree tree, int timestamp) => Uint8List.fromList([
      for (final id in [420, 43, 42, 1]) ...tree.produce(id, timestamp),
    ]);
    check(round(straight, 10).equalsBytes(round(first, 10)), 'same spec');
    final snapshot = first.snapshot();
    first.free();
    final second = bc.restoreChainTree(fx.node(), snapshot);
    check(second.snapshot().equalsBytes(snapshot), 'snapshot round trips');
    check(second.height(1) == 1 && second.height(420) == 1, 'heights');
    check(
      round(straight, 11).equalsBytes(round(second, 11)),
      'a restored tree continues byte for byte',
    );
    check(second.tip(1).equalsBytes(straight.tip(1)), 'root tips agree');
    check(
      second.verify(second.proveAccount(420, fx.alice)),
      'proofs from the restored tree verify',
    );
    check(
      failsAnyhow(
        () => bc.restoreChainTree(
          fx.node(),
          Uint8List.fromList([...snapshot, 1]),
        ),
      ),
      'trailing bytes are refused',
    );
    second.free();
    straight.free();
  });
}
