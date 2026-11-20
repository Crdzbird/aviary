import 'dart:typed_data';

import 'package:aviary/aviary.dart';

/// Passphrase sealing: an account and a threshold share card sealed under a
/// passphrase with Argon2id, a wrong passphrase failing closed, and a memory
/// cost that is not whole KiB refused, as CipherBird 1.6.0 requires.
final class SealingDemo {
  /// A demo that seals a fresh account and share card with [bc].
  const factory SealingDemo(Aviary bc) = SealingDemo._;

  const SealingDemo._(this.bc);

  final Aviary bc;

  static const _ops = 3;
  static const _mem = 1 << 16;

  /// Runs the scenario and returns one line per step.
  List<String> run() {
    final log = <String>[];
    final scratch = bc.createNode(77);
    final acct = scratch.newAccount();
    scratch.dispose();
    final env = bc.sealAccountPassphrase(
      acct.publicKey,
      acct.secret,
      'backup phrase',
      ops: _ops,
      mem: _mem,
    );
    final back = bc.openAccountPassphrase(env, 'backup phrase');
    log.add(
      'account sealed in ${env.length} bytes (Argon2id, 64 KiB) and opened: '
      '${_same(back.secret, acct.secret) ? 'same key ok' : 'FAILED'}',
    );
    log.add(
      'wrong passphrase: '
      '${_fails(() => bc.openAccountPassphrase(env, 'guess')) ? 'refused ok' : 'opened x'}',
    );
    log.add(
      'memory cost 64 KiB + 1 byte: '
      '${_fails(() => bc.sealAccountPassphrase(acct.publicKey, acct.secret, 'backup phrase', ops: _ops, mem: _mem + 1)) ? 'refused ok (whole KiB only)' : 'accepted x'}',
    );
    final cards = bc.createThresholdAccount(3, 2);
    final sealed = bc.sealShare(
      cards.first.bytes,
      'share phrase',
      ops: _ops,
      mem: _mem,
    );
    final card = bc.unsealShare(sealed, 'share phrase');
    log.add(
      '2 of 3 threshold share sealed and unsealed: '
      '${_same(card, cards.first.bytes) ? 'card back ok' : 'FAILED'}',
    );
    return log;
  }

  static bool _same(Uint8List a, Uint8List b) =>
      a.length == b.length &&
      Iterable<int>.generate(a.length).every((i) => a[i] == b[i]);

  static bool _fails(Object? Function() f) {
    try {
      f();
      return false;
    } on AviaryException {
      return true;
    }
  }
}
