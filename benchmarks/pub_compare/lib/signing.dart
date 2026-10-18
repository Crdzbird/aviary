import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:cryptography/dart.dart';
import 'package:ed25519_edwards/ed25519_edwards.dart' as ed;
import 'package:pinenacl/ed25519.dart' as nacl;

import 'measure.dart';

/// The 88 byte body a hand built ledger signs: from, to, amount, nonce and
/// chain id.
Uint8List transferBody(Uint8List from, Uint8List to, int amount, int nonce) {
  final b = Uint8List(88)
    ..setAll(0, from)
    ..setAll(32, to);
  ByteData.sublistView(b)
    ..setUint64(64, amount)
    ..setUint64(72, nonce)
    ..setUint64(80, 1);
  return b;
}

/// Ed25519 signing and verification, and aviary's signed transfer admission.
Future<Map<String, Future<double> Function()>> signing(Aviary bc) async {
  final msg = transferBody(bytesOf(32, 1), bytesOf(32, 2), 5, 0);
  final edKey = ed.generateKey();
  final edSig = ed.sign(edKey.privateKey, msg);
  final naclKey = nacl.SigningKey.generate();
  final naclSig = naclKey.sign(msg).signature;
  final dartEd = DartEd25519();
  final cKey = await dartEd.newKeyPair();
  final cSig = await dartEd.sign(msg, keyPair: cKey);
  return {
    'Ed25519 sign|ed25519_edwards': () async =>
        rate((_) => consume(ed.sign(edKey.privateKey, msg))),
    'Ed25519 verify|ed25519_edwards': () async => rate((_) {
      sink ^= ed.verify(edKey.publicKey, msg, edSig) ? 1 : 0;
    }),
    'Ed25519 sign|pinenacl': () async =>
        rate((_) => consume(naclKey.sign(msg))),
    'Ed25519 verify|pinenacl': () async => rate((_) {
      final ok = naclKey.verifyKey.verify(signature: naclSig, message: msg);
      sink ^= ok ? 1 : 0;
    }),
    'Ed25519 sign|cryptography (pure Dart)': () => rateAsync((_) async {
      consume((await dartEd.sign(msg, keyPair: cKey)).bytes);
    }),
    'Ed25519 verify|cryptography (pure Dart)': () => rateAsync((_) async {
      sink ^= await dartEd.verify(msg, signature: cSig) ? 1 : 0;
    }),
    'Ed25519 transfer: sign, verify, admit|aviary_dart (engine)': () async =>
        admitRate(bc),
  };
}

/// Transfers per second through `submitTransfer`, which signs, verifies and
/// admits each one; blocks drain the mempool outside the timed region.
double admitRate(Aviary bc) {
  const batches = 2, perBatch = 1000;
  final node = bc.createNode(1);
  final from = [for (var i = 0; i < batches * perBatch; i++) node.newAccount()];
  for (final a in from) {
    node.credit(a.address, 1000);
  }
  final to = from.first.address;
  var spent = 0.0;
  for (var b = 0; b < batches; b++) {
    spent += seconds(() {
      for (var i = b * perBatch; i < (b + 1) * perBatch; i++) {
        consume(node.submitTransfer(from[i].secret, to, 1, 0));
      }
    });
    node.produceBlock(b + 2);
  }
  node.free();
  return batches * perBatch / spent;
}
