// Cross-language determinism check (Dart side). Loads libblockchainer_c and
// asserts the SAME golden vectors the C++ test and the Go demo assert.
import 'dart:io';
import 'dart:typed_data';

import 'package:blockchainer_dart/blockchainer_ffi.dart';

String _hex(Uint8List b) =>
    b.map((x) => x.toRadixString(16).padLeft(2, '0')).join();

Uint8List _unhex(String s) {
  final out = Uint8List(s.length ~/ 2);
  for (var i = 0; i < out.length; i++) {
    out[i] = int.parse(s.substring(i * 2, i * 2 + 2), radix: 16);
  }
  return out;
}

void main(List<String> args) {
  // Resolve libblockchainer_c.dylib: CLI arg, env, or default build location.
  final libPath = args.isNotEmpty
      ? args[0]
      : Platform.environment['BLOCKCHAINER_LIB'] ??
          Platform.script
              .resolve('../../../../build/libblockchainer_c.dylib')
              .toFilePath();

  final bc = Blockchainer.load(libPath);
  bc.init();

  print('blockchainer  version : ${bc.version()}');
  print('crypto backend version: ${bc.cryptoBackendVersion()}');

  var failures = 0;
  void check(bool cond, String what) {
    print('  ${cond ? 'ok  ' : 'FAIL'} $what');
    if (!cond) failures++;
  }

  final empty = _hex(bc.hash(Uint8List(0)));
  check(
      empty ==
          'af1349b9f5f9a1a6a0404dea36dcc9499bcb25c9adc112b7cc9a93cae41f3262',
      "BLAKE3('') golden vector");

  final tag = _hex(bc.hash(Uint8List.fromList('blockchainer'.codeUnits)));
  check(tag == 'c9ab2bfe296411d91948b0afc12eda63d2a7ef470e95eb1cb390c58821d8c8ce',
      "BLAKE3('blockchainer') cross-language vector");

  // Replay the canonical txid / header_hash golden vectors through the C ABI.
  final txid = _hex(bc.txid(
    version: 1,
    chainId: 7,
    body: Uint8List.fromList('hello'.codeUnits),
    author: Uint8List(32)..fillRange(0, 32, 0x11),
    signature: Uint8List(64)..fillRange(0, 64, 0x22),
  ));
  check(txid == 'be99a206b50a87b74e8359ab3a2895ed2f082169089b7ff9ec416d9df18d36dd',
      'canonical txid golden vector');

  final headerHash = _hex(bc.headerHash(
    version: 1,
    chainId: 7,
    height: 42,
    parentHash: Uint8List(32),
    timestamp: 1000,
    txRoot: Uint8List(32)..fillRange(0, 32, 0xAA),
    stateRoot: Uint8List(32)..fillRange(0, 32, 0xBB),
    consensusData: Uint8List(0),
  ));
  check(headerHash == '0ad7e2e233a9ebf827717d33b72a685eb993d8bac70b3cee517ba8023dd79793',
      'canonical header_hash golden vector');

  // Verify a state-inclusion proof (golden from vectors/proof_vectors.txt).
  final stateRoot = _unhex(
      '9918413a2ad25fc3ec37457f522bcf091f0aaefc14af15d56b3cb2100632bce3');
  final stateProof = _unhex(
      '01000000381111111111111111111111111111111111111111111111111111111111111111000000000000000000000000000001f40000000000000000000000020000000000000001bb53fe611991a376f327f60748faf0ceaae4671df4d5fb7a89b8f56cbce17033');
  check(bc.verifyStateProof(stateRoot, stateProof), 'state proof verifies');
  final tampered = Uint8List.fromList(stateProof)..[40] ^= 0x01;
  check(!bc.verifyStateProof(stateRoot, tampered), 'tampered state proof rejected');

  print(failures == 0 ? '\nALL PASS' : '\n$failures FAILURE(S)');
  exit(failures == 0 ? 0 : 1);
}
