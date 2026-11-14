@TestOn('browser')
library;

import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/pinned_ledger.dart';
import 'support/web_engine.dart';
import 'support/wide_integers.dart';

Uint8List fill(int byte, int n) => Uint8List(n)..fillRange(0, n, byte);

void main() {
  late Aviary bc;
  setUpAll(() async => bc = await loadWebEngine());

  test('names the build and the CipherBird revision it bundles', () {
    expect(bc.version(), matches(RegExp(r'^\d+\.\d+\.\d+')));
    expect(bc.cryptoBackend(), contains('(CipherBird '));
    expect(bc.cryptoBackendVersion(), bc.cryptoBackend());
    expect(Aviary.loadedFrom, 'web');
    expect(Aviary.isInitialized && Aviary.initialized, isTrue);
  });

  test('BLAKE3 replays vectors/hash_blake3.txt', () {
    expect(bc.hash(Uint8List(0)).hex, pinnedLedger['hash:']);
    expect(
      bc.hash('617669617279'.hexBytes).hex,
      pinnedLedger['hash:617669617279'],
    );
    final big = Uint8List(1 << 20);
    expect(bc.hash(big), hasLength(32));
  });

  test('txid and header hash replay vectors/block_vectors.txt', () {
    final txid = bc.txid(
      1,
      7,
      'hello'.utf8Bytes,
      fill(0x11, 32),
      fill(0x22, 64),
    );
    expect(txid.hex, pinnedLedger['txid']);
    final header = bc.headerHash(
      1,
      7,
      42,
      Uint8List(32),
      1000,
      fill(0xAA, 32),
      fill(0xBB, 32),
      Uint8List(0),
    );
    expect(header.hex, pinnedLedger['header_hash']);
  });

  test('a header hash needs 32 byte roots and non negative integers', () {
    final ok = Uint8List(32);
    expect(
      () => bc.headerHash(1, 7, 42, Uint8List(31), 1000, ok, ok, Uint8List(0)),
      throwsA(
        isA<AviaryException>().having(
          (e) => e.status,
          'status',
          AviaryStatus.badInput,
        ),
      ),
    );
    expect(
      () => bc.headerHash(1, -1, 42, ok, 1000, ok, ok, Uint8List(0)),
      throwsRangeError,
    );
  });

  test('64 bit fields above 32 bits reach the engine whole', () {
    expect(wideHeader(bc.headerHash).hex, wideHeaderHash);
    expect(wideTxid(bc.txid).hex, wideTxidHash);
  });
}
