// Host verification of the preload + lazy-synchronous-singleton contract.
//
//   BLOCKCHAINER_DYLIB=/abs/build/libblockchainer_c.dylib \
//     dart run test/preload_smoke.dart
//
// Asserts: the synchronous API works with NO preload (lazy self-init), preload()
// warms off a background isolate and returns true, and the API stays synchronous
// (no await on any ledger call) and consistent after preload.
import 'dart:typed_data';

import 'package:blockchainer_dart/blockchainer.dart';

void _expect(bool ok, String msg) {
  if (!ok) {
    throw StateError('FAIL: $msg');
  }
}

Future<void> main() async {
  // 1. Synchronous API with NO preload — first touch of `instance` opens + inits
  //    the native lib synchronously; a real ledger call needs no await.
  final v = Blockchainer.instance.version();
  _expect(v.isNotEmpty, 'version non-empty (got "$v")');
  final node = Blockchainer.instance.createNode(42);
  final a = node.newAccount();
  node.credit(a.address, 1000);
  node.submitTransfer(a.secret, node.newAccount().address, 100, node.nonce(a.address));
  final h = node.produceBlock(1);
  _expect(h == 1, 'first block height == 1 (got $h)');
  node.dispose();
  print('  [1] sync API works with no preload — version=$v height=$h');

  // 2. preload() warms in a background isolate and returns true.
  final ok = await Blockchainer.preload();
  _expect(ok, 'preload() returned true');
  print('  [2] preload() warmed off-isolate → $ok');

  // 3. Idempotent; API still synchronous + consistent after preload.
  _expect(await Blockchainer.preload(), 'preload idempotent');
  final eth1 = Blockchainer.instance.ethAddress(_seed(0xab));
  final eth2 = Blockchainer.instance.ethAddress(_seed(0xab));
  _expect(eth1 == eth2, 'deterministic ethAddress after preload');
  _expect(eth1.startsWith('0x') && eth1.length == 42, 'eth address shape ($eth1)');
  print('  [3] sync + deterministic after preload — eth=$eth1');

  print('blockchainer preload smoke: OK');
}

Uint8List _seed(int b) => Uint8List.fromList(List.filled(32, b));
