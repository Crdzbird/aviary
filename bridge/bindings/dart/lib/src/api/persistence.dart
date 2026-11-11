part of '../../aviary.dart';

/// Bringing chains and chain trees back from a snapshot.
/// {@category Chains}
extension AviaryPersistence on Aviary {
  /// Why [restoreChain] would refuse [snapshot] under [genesis]; empty means
  /// it would be accepted.
  String chainSnapshotError(Uint8List genesis, Uint8List snapshot) =>
      String.fromCharCodes(
        _buf2Op('aviary_chain_snapshot_error', genesis, snapshot),
      );

  /// Rebuilds a chain from the genesis it was assembled from and a snapshot,
  /// as validator [me]. A restored chain produces byte for byte what the
  /// original would. Throws with the reason when the snapshot is refused.
  Chain restoreChain(
    Uint8List genesis,
    int me,
    Uint8List blsPub,
    Uint8List blsSec,
    Uint8List snapshot,
  ) {
    final h = _scoped(
      [genesis, blsPub, blsSec, snapshot],
      (p) =>
          lib
              .fn<_ChainRestoreC>('aviary_chain_restore')
              .asFunction<_ChainRestoreD>()(
            p[0],
            genesis.length,
            me,
            p[1],
            blsPub.length,
            p[2],
            blsSec.length,
            p[3],
            snapshot.length,
          ),
    );
    if (h != nullptr) return Chain._(this, h);
    final reason = chainSnapshotError(genesis, snapshot);
    throw AviaryException('chain restore refused: $reason');
  }

  /// Rebuilds a family from the same [root] it was built from and a snapshot
  /// taken with [ChainTreeQueries.snapshot]. Hooks are not restored.
  ChainTree restoreChainTree(TreeNode root, Uint8List snapshot) {
    final spec = encodeTreeNode(root);
    final h = _scoped(
      [spec, snapshot],
      (p) =>
          lib
              .fn<_TreeRestoreC>('aviary_chain_tree_restore')
              .asFunction<_TreeRestoreD>()(
            p[0],
            spec.length,
            p[1],
            snapshot.length,
          ),
    );
    if (h == nullptr) {
      throw const AviaryException('chain tree: restore refused');
    }
    return ChainTree._(this, h);
  }
}
