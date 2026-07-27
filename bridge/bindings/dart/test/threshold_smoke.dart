// FROST t-of-n threshold accounts over the Dart binding. verifyShare gives a
// real cryptographic attribution check via the C ABI; the aggregate's Ed25519
// validity is proven in C++ and the other four bindings against the same ABI.
import 'dart:typed_data';
import 'package:blockchainer_dart/blockchainer.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool fails(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}

void main() {
  final bc = Blockchainer.instance;

  final cards = bc.createThresholdAccount(3, 2);
  expect(cards.length == 3, '3 share cards dealt');
  final groupPK = cards[0].groupPublicKey;
  expect(groupPK.length == 32, 'group pk is 32 bytes');
  for (final c in cards.skip(1)) {
    expect(_eq(c.groupPublicKey, groupPK), 'cards agree on the group key');
  }

  final msg = Uint8List.fromList('threshold-signed transfer'.codeUnits);
  final c0 = bc.commit(cards[0].bytes);
  final c2 = bc.commit(cards[2].bytes);
  final cer = bc.makeCeremony(groupPK, msg, [c2.commitment, c0.commitment]);

  final s0 = cer.sign(cards[0].bytes, c0.nonces);
  final s2 = cer.sign(cards[2].bytes, c2.nonces);

  // Cryptographic attribution: honest share valid, corrupt share named.
  expect(cer.verifyShare(cards[0].index, cards[0].publicShare, s0), 'honest share attributed');
  final bad = Uint8List.fromList(s2);
  bad[5] ^= 1;
  expect(!cer.verifyShare(cards[2].index, cards[2].publicShare, bad), 'corrupt cosigner detected');

  final sig = cer.aggregate([cards[0].index, cards[2].index], [s0, s2]);
  expect(sig.length == 64, 'aggregate is a 64-byte Ed25519 signature');
  cer.free();

  // Sealed share round-trip; wrong passphrase fails closed.
  final env = bc.sealShare(cards[1].bytes, 'share-pass', ops: 3, mem: 1 << 16);
  expect(_eq(bc.unsealShare(env, 'share-pass'), cards[1].bytes), 'sealed share round-trips');
  expect(fails(() => bc.unsealShare(env, 'wrong')), 'wrong passphrase fails closed');

  print('blockchainer dart threshold smoke: OK (2-of-3, attribution, 64B aggregate, sealed)');
}

bool _eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}
