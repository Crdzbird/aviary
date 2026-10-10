import 'dart:io';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void _expect(bool ok, String msg) {
  if (!ok) {
    throw StateError('FAIL: $msg');
  }
}

void main() {
  test('preload', () async {
    final v = Aviary.instance.version();
    _expect(v.isNotEmpty, 'version non-empty (got "$v")');
    final node = Aviary.instance.createNode(42);
    final a = node.newAccount();
    node.credit(a.address, 1000);
    node.submitTransfer(
      a.secret,
      node.newAccount().address,
      100,
      node.nonce(a.address),
    );
    final h = node.produceBlock(1);
    _expect(h == 1, 'first block height == 1 (got $h)');
    node.dispose();
    print('  [1] sync API works with no preload - version=$v height=$h');

    final ok = await Aviary.preload();
    _expect(ok, 'preload() returned true');
    print('  [2] preload() warmed off-isolate -> $ok');

    _expect(await Aviary.preload(), 'preload idempotent');
    final eth1 = Aviary.instance.ethAddress(_seed(0xab));
    final eth2 = Aviary.instance.ethAddress(_seed(0xab));
    _expect(eth1 == eth2, 'deterministic ethAddress after preload');
    _expect(
      eth1.startsWith('0x') && eth1.length == 42,
      'eth address shape ($eth1)',
    );
    print('  [3] sync + deterministic after preload - eth=$eth1');

    print('aviary preload smoke: OK');
    final from = Aviary.loadedFrom;
    if (from == null) throw StateError('FAIL: loadedFrom unset');
    if (Platform.isLinux && from != 'asset') {
      throw StateError(
        'FAIL: on Linux the build hook asset must win, got $from',
      );
    }
    print('engine loaded from: $from');
  });
}

Uint8List _seed(int b) => Uint8List.fromList(List.filled(32, b));
