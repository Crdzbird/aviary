part of '../../aviary.dart';

/// Read only views and proofs of a chain tree.
/// {@category Subchains and trees}
extension ChainTreeQueries on ChainTree {
  /// The family's chain ids, parents first.
  List<int> chainIds() {
    final b = _bc._take(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(
        'aviary_chain_tree_chain_ids',
      )(_h),
      'chain_tree_chain_ids',
    );
    final bd = ByteData.sublistView(b);
    return List.generate(bd.getUint32(0), (i) => bd.getUint64(4 + 8 * i));
  }

  /// The height of a chain.
  int height(int chainId) => _bc.lib.lookupFunction<_TreeU64IdC, _TreeU64IdD>(
    'aviary_chain_tree_height',
  )(_h, chainId);

  Uint8List _byChain(String fn, int chainId) => _bc._take(
    _bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>(fn)(_h, chainId),
    fn,
  );

  /// The final genesis of a chain, with policy and binding committed.
  Uint8List genesis(int chainId) =>
      _byChain('aviary_chain_tree_genesis', chainId);

  /// The current tip header of a chain.
  Uint8List tip(int chainId) => _byChain('aviary_chain_tree_tip', chainId);

  /// How many anchors the next block on a chain would carry.
  int pendingAnchors(int chainId) =>
      _bc.lib.lookupFunction<_TreeU32IdC, _TreeU32IdD>(
        'aviary_chain_tree_pending_anchors',
      )(_h, chainId);

  /// A cross chain proof of [address] on [chainId] from the root header.
  Uint8List proveAccount(int chainId, Uint8List address) => _scoped(
    [address],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_TreeProveC, _TreeProveD>(
        'aviary_chain_tree_prove_account',
      )(_h, chainId, p[0]),
      'chain_tree_prove_account',
    ),
  );
}
