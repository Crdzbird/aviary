part of '../aviary_web.dart';

/// Proofs against a dynamic registry.
/// {@category Subchains and trees}
extension AviaryChildProofs on Aviary {
  /// Parses the fixed 230 byte record layout.
  ChildRecord decodeChildRecord(Uint8List record) => childRecordOf(record);

  /// Checks a child proof against a 32 byte version 4 subchain root taken
  /// from a parent header the caller trusts, and returns the proven record.
  /// Read it: its tip, and whether the child is open. Throws when the proof
  /// does not hold.
  ChildRecord verifyChildProof(Uint8List subchainRoot, Uint8List proof) {
    if (subchainRoot.length != 32) {
      throw const AviaryException('subchain root must be 32 bytes');
    }
    return childRecordOf(
      _engine.scoped(
        [subchainRoot, proof],
        (p) => _engine.take('aviary_child_proof_verify', [
          p[0],
          p[1],
          proof.length,
        ], 'child_proof_verify'),
      ),
    );
  }

  /// Whether [proof] is committed under a parent's 32 byte subchain root all
  /// the way down and its state proof holds.
  bool verifySubchainProofV2(Uint8List parentSubchainRoot, Uint8List proof) {
    if (parentSubchainRoot.length != 32) return false;
    return _engine.scoped(
      [parentSubchainRoot, proof],
      (p) =>
          _engine.call('aviary_subchain_proof_v2_verify', [
            p[0],
            p[1],
            proof.length,
          ]) ==
          1,
    );
  }
}
