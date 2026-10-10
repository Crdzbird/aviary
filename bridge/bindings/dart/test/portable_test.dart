import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void _expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool _eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void main() {
  test('portable', () {
    final bc = Aviary.instance;

    final node = bc.createNode(7);
    final acct = node.newAccountHybrid();
    final public = acct.publicKey, secret = acct.secret;

    final env1 = bc.sealAccountPassphrase(
      public,
      secret,
      'backup-phrase',
      ops: 3,
      mem: 1 << 16,
    );
    final back1 = bc.openAccountPassphrase(env1, 'backup-phrase');
    _expect(
      _eq(back1.secret, secret) && _eq(back1.public, public),
      'passphrase round-trip',
    );
    var threw = false;
    try {
      bc.openAccountPassphrase(env1, 'wrong');
    } on Object {
      threw = true;
    }
    _expect(threw, 'wrong passphrase fails closed');

    final dev = bc.hybridKemKeypair();

    final env2 = bc.sealAccountTo(public, secret, dev.public);
    _expect(
      _eq(bc.openAccountTo(env2, dev.secret).secret, secret),
      'recipient round-trip',
    );
    final stranger = bc.hybridKemKeypair();
    threw = false;
    try {
      bc.openAccountTo(env2, stranger.secret);
    } on Object {
      threw = true;
    }
    _expect(threw, 'a different device cannot open a recipient envelope');

    final env3 = bc.sealAccountSignedTo(public, secret, dev.public, secret);
    _expect(
      _eq(bc.openAccountSignedFrom(env3, dev.secret, public).secret, secret),
      'signed-recipient round-trip',
    );
    threw = false;
    try {
      bc.openAccountSignedFrom(env3, dev.secret, stranger.public);
    } on Object {
      threw = true;
    }
    _expect(threw, 'wrong expected signer fails closed');

    node.dispose();
    print(
      'aviary Dart portable smoke: OK '
      '(passphrase + recipient + signed tiers, all fail-closed)',
    );
  });
}
