@TestOn('vm')
library;

import 'dart:io';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'web/support/commit_fixture.dart';
import 'web/support/pinned_ledger.dart';
import 'web/support/pinned_subchain.dart';
import 'web/support/wide_integers.dart';

/// The name and value columns of a vectors file; null outside the repository.
Map<String, String>? vectorLines(String name) {
  final file = File('../../../vectors/$name');
  if (!file.existsSync()) return null;
  final out = <String, String>{};
  for (final raw in file.readAsLinesSync()) {
    final line = raw.trim();
    if (line.isEmpty || line.startsWith('#')) continue;
    final fields = line.split(RegExp(r'\s+'));
    out[fields[0]] = fields[1];
  }
  return out;
}

void main() {
  test('the browser copies match the vector files', () {
    final hash = vectorLines('hash_blake3.txt');
    if (hash == null) {
      markTestSkipped('the vectors directory is not available');
      return;
    }
    final ledger = {
      for (final e in hash.entries)
        'hash:${e.key == '""' ? '' : e.key}': e.value,
      ...vectorLines('block_vectors.txt')!,
      ...vectorLines('proof_vectors.txt')!,
    };
    for (final e in pinnedLedger.entries) {
      expect(ledger[e.key], e.value, reason: e.key);
    }
    final subchain = vectorLines('subchain_v2_vectors.txt')!;
    for (final e in pinnedSubchain.entries) {
      expect(subchain[e.key], e.value, reason: e.key);
    }
  });

  test('the native engine agrees with the browser fixtures', () {
    final bc = Aviary.instance;
    final vset = commitFixture['validator_set']!.hexBytes;
    final cert = commitFixture['certificate']!.hexBytes;
    expect(bc.verifyCommit(commitFixtureChainId, vset, cert), isTrue);
    expect(bc.verifyCommit(commitFixtureChainId + 1, vset, cert), isFalse);
    final header = bc.blockHeader(commitFixture['block']!.hexBytes);
    expect(header.hex, commitFixture['header']);
    expect(bc.encodedHeaderHash(header).hex, commitFixture['header_hash']);
    expect(wideHeader(bc.headerHash).hex, wideHeaderHash);
    expect(wideTxid(bc.txid).hex, wideTxidHash);
  });

  test('the engine names the CipherBird revision it was built from', () {
    expect(Aviary.instance.cryptoBackend(), contains('(CipherBird '));
  });
}
