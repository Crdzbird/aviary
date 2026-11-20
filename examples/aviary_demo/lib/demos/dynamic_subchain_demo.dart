import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary/aviary.dart';

import 'record_family.dart';

/// Subchains version 2: a parent registers a child at runtime through a
/// registrar signed operation, anchors it, and proves both the child's
/// registry record and a record written on the child from its own header.
final class DynamicSubchainDemo {
  /// A demo over [family], whose chains it builds and frees on each run.
  const factory DynamicSubchainDemo(RecordFamily family) =
      DynamicSubchainDemo._;

  const DynamicSubchainDemo._(this.family);

  final RecordFamily family;

  /// Runs the scenario and returns one line per step.
  List<String> run() {
    final bc = family.bc;
    final log = <String>[];
    final parentGen = family.parentGenesis(7);
    final parentHash = bc.genesisHash(parentGen);
    final childGen = family.childGenesis(7001, 7, parentHash);
    final parent = family.assemble(parentGen, family.parent);
    final child = family.assemble(childGen, family.child);
    try {
      log.add(
        'parent 7 starts with ${parent.registryCounts()?.records} children',
      );
      final op = family.register(7001, childGen, 7, parentHash);
      family.commit(parent, parent.proposeSubchain(10, ops: [op]));
      final counts = parent.registryCounts();
      log.add(
        'registrar signed op registers chain 7001 in parent block '
        '${parent.height}: ${counts?.records} record, ${counts?.open} open ok',
      );

      final key = Uint8List(32)..fillRange(0, 32, 0x42);
      child.addTx(
        bc.signRecordTx(
          chainId: 7001,
          nonce: 0,
          key: key,
          op: RecordOp.put,
          value: utf8.encode('door opened'),
          secret: family.registrarSecret,
        ),
      );
      family.commit(child, child.propose(20));
      final anchor = bc.pendingAnchor(parent, child)!;
      family.commit(parent, parent.proposeSubchain(11, anchors: [anchor]));
      log.add('child block 1 holds a record; parent block 2 anchors it');

      final root = bc.headerSubchainRoot(parent.tip());
      final record = bc.verifyChildProof(root, parent.childProof(7001));
      log.add(
        'child record proven from the parent header: tip height '
        '${record.tipHeight}, status ${record.status.name} ok',
      );
      final proof = bc.encodeSubchainProofV2([
        parent.subchainHopV2(7001),
      ], child.proveRecord(key));
      final tampered = Uint8List.fromList(proof)..[proof.length - 1] ^= 1;
      log.add(
        'record on chain 7001 proven from the parent header: '
        '${bc.verifySubchainProofV2(root, proof) ? 'verified ok' : 'FAILED'}; '
        'tampered: '
        '${bc.verifySubchainProofV2(root, tampered) ? 'accepted x' : 'rejected ok'}',
      );
      return log;
    } finally {
      parent.free();
      child.free();
    }
  }
}
