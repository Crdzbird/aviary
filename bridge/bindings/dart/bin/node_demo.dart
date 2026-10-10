import 'dart:convert';
import 'dart:io';
import 'dart:typed_data';

import 'package:blockchainer_dart/blockchainer.dart';

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
  final libPath = args.isNotEmpty
      ? args[0]
      : Platform.script
          .resolve('../../../../build/libblockchainer_c.dylib')
          .toFilePath();
  final bc = Blockchainer.open(libPath);

  print(
      'blockchainer ${bc.version()} (crypto backend ${bc.cryptoBackend()})\n');

  final node = bc.createNode(42);
  final alice = node.newAccount();
  final bob = node.newAccount();
  node.credit(alice.address, 1000);
  print('created chain 42; alice funded 1000');
  print(
      '  alice ${_hex(alice.address).substring(0, 16)}...  balance=${node.balance(alice.address)}');

  node.submitTransfer(alice.secret, bob.address, 100, 0);
  node.submitTransfer(alice.secret, bob.address, 50, 1);
  print('\nsubmitted 2 transfers; mempool=${node.mempoolSize()}');
  final h = node.produceBlock(1);
  print('produced block -> height=$h');
  print(
      '  alice=${node.balance(alice.address)}  bob=${node.balance(bob.address)}');

  print('\n-- streamed events --');
  for (final e in jsonDecode(node.drainEvents()) as List) {
    print('  $e');
  }

  print('\n-- metrics (Prometheus) --');
  for (final line in node.metrics().split('\n')) {
    if (line.isNotEmpty && !line.startsWith('#')) print('  $line');
  }

  final proof = node.proveAccount(alice.address);
  final ok = bc.verifyStateProof(node.stateRoot(), proof);
  print('\nlight-client proof of alice verifies: $ok');

  final ethKey = _unhex(
      'ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80');
  print('\n-- multi-chain interop addresses --');
  print('  ETH: ${bc.ethAddress(ethKey)}');
  print(
      '  BTC: ${bc.btcAddress(_unhex('0000000000000000000000000000000000000000000000000000000000000001'))}');
  print('  SOL: ${bc.solAddress(alice.secret)}');

  node.dispose();
  print('\nALL GOOD');
  exit(0);
}
