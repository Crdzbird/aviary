part of '../../aviary.dart';

/// Chain trees: a declared family of chains built and driven together.
extension AviaryTrees on Aviary {
  /// Canonically encodes a tree node, children first.
  Uint8List encodeTreeNode(TreeNode n) {
    final bp =
        (n.blueprint ?? makeBlueprint(consensus: 'bft', stateModel: 'account'))
            .encoded;
    final secrets = _concat(n.validators.map((v) => v.secret ?? Uint8List(32)));
    final pks = _concat(n.validators.map((v) => v.publicKey));
    final kids = _blobList([for (final c in n.children) encodeTreeNode(c)]);
    return _withU64s(
      [for (final a in n.accounts) a.balance],
      (bal) => _withU64s(
        [for (final v in n.validators) v.power],
        (pw) => _scoped(
          [bp, _concat(n.accounts.map((a) => a.address)), pks, secrets, kids],
          (p) => _take(
            lib.lookupFunction<_TreeNodeC, _TreeNodeD>(
                    'aviary_chain_tree_node_encode')(
                n.chainId,
                p[0],
                bp.length,
                n.assuranceLevel,
                p[1],
                bal,
                n.accounts.length,
                p[2],
                p[3],
                pw,
                n.validators.length,
                n.maxAnchorsPerBlock,
                n.maxHeadersPerAnchor,
                p[4],
                kids.length),
            'chain_tree_node_encode',
          ),
        ),
      ),
    );
  }

  /// Why a spec is structurally invalid; empty means valid.
  String chainTreeSpecError(Uint8List spec) =>
      String.fromCharCodes(_bufOp('aviary_chain_tree_spec_error', spec));

  /// Builds a family from [root]. Throws with the structural reason, or a
  /// generic reject when the registry or posture refuses.
  ChainTree buildChainTree(TreeNode root) {
    final spec = encodeTreeNode(root);
    final err = chainTreeSpecError(spec);
    if (err.isNotEmpty) throw StateError('chain tree: $err');
    return _scoped([spec], (p) {
      final h = lib.lookupFunction<_TreeBuildC, _TreeBuildD>(
          'aviary_chain_tree_build')(p[0], spec.length);
      if (h == nullptr) throw StateError('chain tree: build rejected');
      return ChainTree._(this, h);
    });
  }
}
