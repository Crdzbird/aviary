import 'dart:typed_data';

import 'package:aviary/aviary.dart';

import 'record_family.dart';

/// Chain persistence: snapshot a chain, restore it, and show that the
/// restored chain carries on byte for byte; read and prune the block
/// archive; and snapshot the dashboard's own node.
final class PersistenceDemo {
  /// A demo over [family] that also snapshots the dashboard's [node].
  const factory PersistenceDemo(RecordFamily family, Node node) =
      PersistenceDemo._;

  const PersistenceDemo._(this.family, this.node);

  final RecordFamily family;
  final Node node;

  /// Runs the scenario and returns one line per step.
  List<String> run() {
    final bc = family.bc;
    final v = family.parent;
    final log = <String>[];
    final genesis = family.genesis(9, v);
    final chain = family.assemble(genesis, v);
    Chain? back;
    try {
      for (var ts = 2; ts <= 4; ts++) {
        family.commit(chain, chain.propose(ts));
      }
      final snap = chain.snapshot();
      log.add(
        'chain 9 at height ${chain.height}: snapshot of ${snap.length} bytes, '
        'check: ${bc.chainSnapshotError(genesis, snap).isEmpty ? 'valid ok' : 'invalid'}',
      );
      final bad = Uint8List.fromList(snap)..[snap.length ~/ 4] ^= 1;
      log.add(
        'a flipped ledger byte: '
        '${bc.chainSnapshotError(genesis, bad).isEmpty ? 'accepted x' : 'refused ok'}',
      );
      back = bc.restoreChain(genesis, 0, v.public, v.secret, snap);
      final straight = family.commit(chain, chain.propose(5));
      final resumed = family.commit(back, back.propose(5));
      final same = _same(straight, resumed);
      log.add(
        'restored chain proposes the same block 4 as the original: '
        '${same ? 'identical ok' : 'DIFFERENT'}',
      );
      final range = back.archiveRange()!;
      log.add(
        'archive holds blocks ${range.first}..${range.last}; pruning below 3 '
        'drops ${back.pruneArchive(3)}',
      );
      final copy = bc.restoreNode(node.snapshot());
      final matches = _same(copy.stateRoot(), node.stateRoot());
      log.add(
        'dashboard node restored at height ${copy.height()}: state root '
        '${matches ? 'matches ok' : 'DIFFERS'}',
      );
      copy.dispose();
      return log;
    } finally {
      chain.free();
      back?.free();
    }
  }

  static bool _same(Uint8List a, Uint8List b) =>
      a.length == b.length &&
      Iterable<int>.generate(a.length).every((i) => a[i] == b[i]);
}
