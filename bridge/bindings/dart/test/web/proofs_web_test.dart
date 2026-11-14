@TestOn('browser')
library;

import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/commit_fixture.dart';
import 'support/pinned_ledger.dart';
import 'support/web_engine.dart';

Uint8List flipped(Uint8List b, int at) =>
    Uint8List.fromList(b)..[at] = b[at] ^ 0x01;

void main() {
  late Aviary bc;
  setUpAll(() async => bc = await loadWebEngine());

  test('state proof replays vectors/proof_vectors.txt', () {
    final root = pinnedLedger['state_root']!.hexBytes;
    final proof = pinnedLedger['state_proof']!.hexBytes;
    expect(bc.verifyStateProof(root, proof), isTrue);
    expect(bc.verifyStateProof(flipped(root, 0), proof), isFalse);
    expect(
      bc.verifyStateProof(root, flipped(proof, proof.length - 1)),
      isFalse,
    );
    expect(bc.verifyStateProof(root.sublist(0, 31), proof), isFalse);
    expect(bc.verifyStateProof(root, Uint8List(0)), isFalse);
    expect(bc.verifyTxInclusion(root, Uint8List(5)), isFalse);
  });

  test('a commit certificate verifies under its set and chain only', () {
    final vset = commitFixture['validator_set']!.hexBytes;
    final cert = commitFixture['certificate']!.hexBytes;
    const id = commitFixtureChainId;
    expect(bc.verifyCommit(id, vset, cert), isTrue);
    expect(bc.verifyCommit(id + 1, vset, cert), isFalse);
    expect(bc.verifyCommit(id, vset, flipped(cert, cert.length - 1)), isFalse);
    expect(bc.verifyCommit(id, flipped(vset, 10), cert), isFalse);
    expect(bc.verifyCommit(id, vset, cert.sublist(0, 20)), isFalse);
    expect(bc.verifyCommit(id, Uint8List(0), Uint8List(0)), isFalse);
  });

  test('a block yields its header, header hash and subchain root', () {
    final header = bc.blockHeader(commitFixture['block']!.hexBytes);
    expect(header.hex, commitFixture['header']);
    expect(bc.encodedHeaderHash(header).hex, commitFixture['header_hash']);
    expect(bc.headerSubchainRoot(header).equalsBytes(Uint8List(32)), isTrue);
  });

  test('a malformed block reports the engine error', () {
    expect(
      () => bc.blockHeader(Uint8List.fromList([1, 2, 3])),
      throwsA(
        isA<AviaryException>().having(
          (e) => e.message,
          'message',
          isNotEmpty,
        ),
      ),
    );
    expect(
      () => bc.encodedHeaderHash(Uint8List(0)),
      throwsA(isA<AviaryException>()),
    );
  });
}
