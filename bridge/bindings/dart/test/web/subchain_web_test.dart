@TestOn('browser')
library;

import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/pinned_subchain.dart';
import 'support/web_engine.dart';

Uint8List pin(String name) => pinnedSubchain[name]!.hexBytes;

void main() {
  late Aviary bc;
  setUpAll(() async => bc = await loadWebEngine());

  test('header hash and subchain root replay subchain_v2_vectors.txt', () {
    final header = pin('header_1');
    expect(bc.encodedHeaderHash(header).hex, pinnedSubchain['header_1_hash']);
    expect(
      bc.headerSubchainRoot(header).hex,
      pinnedSubchain['subchain_root_1'],
    );
  });

  test('a child proof returns the proven record', () {
    final record = bc.verifyChildProof(
      pin('subchain_root_2'),
      pin('child_proof_2'),
    );
    expect(record.encoded.hex, pinnedSubchain['child_proof_2_verified']);
    expect(record.chainId, 1001);
    expect(record.status, ChildStatus.open);
    expect(record.tipHeight, 2);
    final wide = bc.verifyChildProof(pin('wide_root'), pin('wide_proof'));
    expect(wide.encoded.hex, pinnedSubchain['wide_proof_verified']);
    expect(wide.chainId, 2007);
    expect(wide.index, 7);
    final decoded = bc.decodeChildRecord(wide.encoded);
    expect(decoded.genesisHeaderHash.hex, wide.genesisHeaderHash.hex);
  });

  test('a child proof under another root throws', () {
    expect(
      () => bc.verifyChildProof(pin('subchain_root_1'), pin('child_proof_2')),
      throwsA(isA<AviaryException>()),
    );
    expect(
      () => bc.verifyChildProof(Uint8List(31), pin('child_proof_2')),
      throwsA(isA<AviaryException>()),
    );
    expect(
      () => bc.decodeChildRecord(Uint8List(229)),
      throwsA(isA<AviaryException>()),
    );
  });

  test('a path proof holds under its own root only', () {
    final proof = pin('path_proof_2');
    expect(bc.verifySubchainProofV2(pin('subchain_root_2'), proof), isTrue);
    expect(bc.verifySubchainProofV2(pin('subchain_root_1'), proof), isFalse);
    expect(bc.verifySubchainProofV2(Uint8List(31), proof), isFalse);
  });
}
