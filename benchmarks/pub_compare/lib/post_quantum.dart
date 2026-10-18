import 'package:aviary_dart/aviary.dart';
import 'package:ed25519_edwards/ed25519_edwards.dart' as ed;
import 'package:pqcrypto/pqcrypto.dart';

import 'measure.dart';
import 'signing.dart';

/// ML-DSA-65 alone and as the post quantum half of a hybrid Ed25519 +
/// ML-DSA-65 transfer, against aviary's hybrid transfer admission.
Map<String, Future<double> Function()> postQuantum(Aviary bc) {
  const params = DilithiumParams.mlDsa65;
  final msg = transferBody(bytesOf(32, 3), bytesOf(32, 4), 5, 0);
  final (pk, sk) = MlDsa.generateKeyPair(params);
  final sig = MlDsa.sign(sk, msg, params);
  final edKey = ed.generateKey();
  final edSig = ed.sign(edKey.privateKey, msg);
  return {
    'ML-DSA-65 sign|pqcrypto (pure Dart)': () async =>
        rate((_) => consume(MlDsa.sign(sk, msg, params))),
    'ML-DSA-65 verify|pqcrypto (pure Dart)': () async => rate((_) {
      sink ^= MlDsa.verify(pk, msg, sig, params) ? 1 : 0;
    }),
    'Hybrid transfer: sign and verify both|ed25519_edwards + pqcrypto':
        () async => rate((_) {
          consume(ed.sign(edKey.privateKey, msg));
          consume(MlDsa.sign(sk, msg, params));
          final ok =
              ed.verify(edKey.publicKey, msg, edSig) &&
              MlDsa.verify(pk, msg, sig, params);
          sink ^= ok ? 1 : 0;
        }),
    'Hybrid transfer: sign, verify both, admit|aviary_dart (engine)':
        () async => _hybridAdmitRate(bc),
  };
}

double _hybridAdmitRate(Aviary bc) {
  const n = 300;
  final node = bc.createNode(9);
  final from = [for (var i = 0; i < n; i++) node.newAccountHybrid()];
  for (final a in from) {
    node.credit(a.address, 1000);
  }
  final to = from.first.address;
  final spent = seconds(() {
    for (final a in from) {
      consume(node.submitTransferHybrid(a.secret, a.publicKey, to, 1, 0));
    }
  });
  node.free();
  return n / spent;
}
