part of '../../aviary.dart';

/// Proofs against a dynamic registry, and the anchor a parent takes next.
/// {@category Subchains and trees}
extension AviaryChildProofs on Aviary {
  /// Parses the fixed 230 byte record layout.
  ChildRecord decodeChildRecord(Uint8List record) => _childRecordOf(record);

  /// Checks a child proof against a 32 byte version 4 subchain root taken
  /// from a parent header the caller trusts, and returns the proven record.
  /// Read it: its tip, and whether the child is open. Throws when the proof
  /// does not hold.
  ChildRecord verifyChildProof(Uint8List subchainRoot, Uint8List proof) {
    if (subchainRoot.length != 32) {
      throw const AviaryException('subchain root must be 32 bytes');
    }
    return _childRecordOf(
      _scoped(
        [subchainRoot, proof],
        (p) => _take(
          lib
              .fn<_ChildProofVerifyC>('aviary_child_proof_verify')
              .asFunction<_ChildProofVerifyD>()(p[0], p[1], proof.length),
          'child_proof_verify',
        ),
      ),
    );
  }

  /// The next anchor [parent] can take for [child], built from the child's
  /// archive and the parent's recorded tip, at most [limit] headers where 0
  /// is the policy bound. Null when nothing is pending.
  Uint8List? pendingAnchor(Chain parent, Chain child, {int limit = 0}) {
    final anchor = _take(
      lib
          .fn<_PendingAnchorC>('aviary_chain_pending_anchor')
          .asFunction<_PendingAnchorD>()(parent._h, child._h, limit),
      'chain_pending_anchor',
    );
    if (anchor.isEmpty) return null;
    return anchor;
  }

  /// A cross chain proof through any mixture of parents, from tagged hops
  /// (see [ChainRegistry.subchainHopV2]), top parent first, and the state
  /// proof of the last chain.
  Uint8List encodeSubchainProofV2(List<Uint8List> hops, Uint8List stateProof) =>
      _buf2Op(
        'aviary_subchain_proof_v2_encode',
        hops.isEmpty ? Uint8List(0) : _blobList(hops),
        stateProof,
      );

  /// Whether [proof] is committed under a parent's 32 byte subchain root all
  /// the way down and its state proof holds.
  bool verifySubchainProofV2(Uint8List parentSubchainRoot, Uint8List proof) {
    if (parentSubchainRoot.length != 32) return false;
    return _scoped(
      [parentSubchainRoot, proof],
      (p) =>
          lib
              .fn<_VerifyC>('aviary_subchain_proof_v2_verify')
              .asFunction<_VerifyD>()(p[0], p[1], proof.length) ==
          1,
    );
  }
}
