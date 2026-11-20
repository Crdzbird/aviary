import 'dart:typed_data';

import 'package:aviary/aviary.dart';
import 'package:aviary_demo/demos/dynamic_subchain_demo.dart';
import 'package:aviary_demo/demos/persistence_demo.dart';
import 'package:aviary_demo/demos/record_family.dart';
import 'package:aviary_demo/demos/sealing_demo.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final bc = Aviary.instance;
  final node = bc.createNode(42);
  final alice = node.newAccount();
  node.credit(alice.address, 1000);
  node.produceBlock(1);
  Uint8List seed(int b) => Uint8List(32)..fillRange(0, 32, b);
  final family = RecordFamily(
    bc,
    bc.createNode(0x5501).newAccount().secret,
    bc.newBLSValidator(seed(0x51)),
    bc.newBLSValidator(seed(0x52)),
  );

  void expectClean(List<String> log, int steps) {
    expect(log, hasLength(steps));
    for (final line in log) {
      expect(line, isNot(contains('FAILED')));
      expect(line, isNot(contains(' x')));
    }
  }

  test('dynamic subchain demo', () {
    final log = DynamicSubchainDemo(family).run();
    expectClean(log, 5);
    expect(log.last, contains('verified ok'));
    expect(log.last, contains('rejected ok'));
  });

  test('persistence demo', () {
    final log = PersistenceDemo(family, node).run();
    expectClean(log, 5);
    expect(log[2], contains('identical ok'));
    expect(log.last, contains('matches ok'));
  });

  test('sealing demo', () {
    final log = SealingDemo(bc).run();
    expectClean(log, 4);
    expect(log[2], contains('refused ok'));
  });
}
