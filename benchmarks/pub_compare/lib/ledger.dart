import 'package:aviary_dart/aviary.dart';
import 'package:ed25519_edwards/ed25519_edwards.dart' as ed;

import 'hand_ledger.dart';
import 'measure.dart';

const _accounts = 1000;

/// One block of 1,000 transfers, hand built and in aviary, best of
/// [rounds]: signing and admission, block production, and both together.
Map<String, double> ledger(Aviary bc, {int rounds = 5}) {
  const hand = 'hand built: ed25519_edwards + hashlib + dart_merkle_lib';
  const engine = 'aviary_dart (engine, with BFT certificate)';
  final best = <String, double>{};
  void keep(String key, double txPerSecond) {
    if (txPerSecond > (best[key] ?? 0)) best[key] = txPerSecond;
  }

  final diy = HandLedger([
    for (var i = 0; i < _accounts; i++) ed.generateKey(),
  ]);
  final node = bc.createNode(7);
  final accounts = [for (var i = 0; i < _accounts; i++) node.newAccount()];
  for (final a in accounts) {
    node.credit(a.address, 1000000);
  }
  final to = accounts.first.address;
  final blocks = <List<SignedTransfer>>[];
  for (var r = 0; r < rounds; r++) {
    final admit = seconds(() {
      final signed = diy.signAll();
      blocks.add(signed);
      for (final t in signed) {
        if (!diy.admit(t)) throw StateError('hand built ledger refused');
      }
    });
    final block = seconds(() => consume(diy.produceBlock().$1));
    keep('Sign, verify and admit 1,000 transfers|$hand', _accounts / admit);
    keep('Produce the block of 1,000|$hand', _accounts / block);
    keep('Admit and finalize, end to end|$hand', _accounts / (admit + block));
    final eAdmit = seconds(() {
      for (final a in accounts) {
        consume(node.submitTransfer(a.secret, to, 1, r));
      }
    });
    final eBlock = seconds(() => node.produceBlock(r + 2));
    keep('Sign, verify and admit 1,000 transfers|$engine', _accounts / eAdmit);
    keep('Produce the block of 1,000|$engine', _accounts / eBlock);
    keep(
      'Admit and finalize, end to end|$engine',
      _accounts / (eAdmit + eBlock),
    );
  }
  if (node.height() != rounds) throw StateError('aviary missed a block');
  const follow = 'Follower: verify and apply received blocks';
  final follower = HandLedger(diy.keys);
  final replay = seconds(() {
    for (final b in blocks) {
      for (final t in b) {
        if (!follower.admit(t)) throw StateError('follower refused');
      }
      consume(follower.produceBlock().$2);
    }
  });
  keep('$follow|$hand', rounds * _accounts / replay);
  final snapshot = node.snapshot();
  final restored = seconds(() => bc.restoreNode(snapshot, replay: true).free());
  keep('$follow|$engine', rounds * _accounts / restored);
  node.free();
  return best;
}
