// Verifies the preload + lazy-synchronous-singleton contract.
//
// Run on the host (loads the desktop dylib via the BLOCKCHAINER_DYLIB override;
// libcryptolib_c is resolved via the dylib's rpath):
//   BLOCKCHAINER_DYLIB=/path/to/libblockchainer_c.dylib flutter test
import 'dart:typed_data';

import 'package:blockchainer_flutter/blockchainer_flutter.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('synchronous API works with NO preload (lazy self-init)', () {
    // First touch of `instance` must open + init the lib synchronously.
    final v = Blockchainer.instance.version();
    expect(v, isNotEmpty);
    // A real ledger flow, fully synchronous — no await anywhere.
    final node = Blockchainer.instance.createNode(7);
    final a = node.newAccount();
    node.credit(a.address, 1000);
    node.submitTransfer(a.secret, node.newAccount().address, 10, node.nonce(a.address));
    expect(node.produceBlock(1), 1);
    node.dispose();
  });

  test('preload() warms in a background isolate and returns true', () async {
    expect(await Blockchainer.preload(), isTrue);
  });

  test('synchronous API works (and is consistent) AFTER preload', () async {
    await Blockchainer.preload();
    final bc = Blockchainer.instance; // no await on the calls themselves
    expect(bc.version(), isNotEmpty);
    final seed = Uint8List.fromList(List.filled(32, 0x11));
    expect(bc.ethAddress(seed), bc.ethAddress(seed)); // deterministic
  });

  test('preload is idempotent — repeated calls all succeed', () async {
    expect(await Blockchainer.preload(), isTrue);
    expect(await Blockchainer.preload(), isTrue);
    // And the singleton is still usable synchronously.
    expect(Blockchainer.instance.createNode(9).height(), 0);
  });

  test('ensureInitialized() completes and marks the API ready', () async {
    await Blockchainer.ensureInitialized();
    expect(Blockchainer.isInitialized, isTrue);
    // After awaiting readiness, ledger calls are fully synchronous (no await).
    final node = Blockchainer.instance.createNode(11);
    final a = node.newAccount();
    node.credit(a.address, 500);
    expect(node.produceBlock(1), 1);
  });

  test('ensureInitialized() is idempotent + concurrency-safe', () async {
    // Many concurrent callers share a single background warm-up.
    await Future.wait([for (var i = 0; i < 8; i++) Blockchainer.ensureInitialized()]);
    expect(Blockchainer.isInitialized, isTrue);
  });
}
