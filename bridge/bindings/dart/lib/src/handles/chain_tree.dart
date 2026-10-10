part of '../../aviary.dart';

/// A built family of chains driven in process. [produce] anchors children
/// upward automatically; [ChainTreeQueries.proveAccount] proves any
/// descendant from the root header.
final class ChainTree extends _NativeHandle {
  ChainTree._(Aviary bc, Pointer<Void> h)
    : _handle = _nextTreeHandle++,
      super(bc, h, 'aviary_chain_tree_free') {
    _trees[_handle] = this;
    _fin = NativeCallable<_TreeFinC>.isolateLocal(_treeFinTrampoline)
      ..keepIsolateAlive = false;
    _tx = NativeCallable<_TreeTxC>.isolateLocal(
      _treeTxTrampoline,
      exceptionalReturn: 0,
    )..keepIsolateAlive = false;
    final ctx = Pointer<Void>.fromAddress(_handle);
    _bc.lib.lookupFunction<_TreeSetFinC, _TreeSetFinD>(
      'aviary_chain_tree_set_finalized_hook',
    )(_h, _fin.nativeFunction, ctx);
    _bc.lib.lookupFunction<_TreeSetTxC, _TreeSetTxD>(
      'aviary_chain_tree_set_tx_source',
    )(_h, _tx.nativeFunction, ctx);
  }

  final int _handle;
  late final NativeCallable<_TreeFinC> _fin;
  late final NativeCallable<_TreeTxC> _tx;

  /// Observes every finalized block.
  FinalizedHook? onFinalized;

  /// Supplies transactions when a chain produces.
  TxSource? txSource;

  /// Produces and finalizes the next block on [chainId].
  Uint8List produce(int chainId, int timestamp) => _bc._take(
    _bc.lib.lookupFunction<_TreeProduceC, _TreeProduceD>(
      'aviary_chain_tree_produce',
    )(_h, chainId, timestamp),
    'chain_tree_produce',
  );

  /// Whether [proof] is committed by the root's current header.
  bool verify(Uint8List proof) => _scoped(
    [proof],
    (p) =>
        _bc.lib.lookupFunction<_TreeVerifyC, _TreeVerifyD>(
          'aviary_chain_tree_verify',
        )(_h, p[0], proof.length) ==
        1,
  );

  /// Releases the native tree and its hooks.
  void free() {
    if (isReleased) return;
    _release();
    _trees.remove(_handle);
    _fin.close();
    _tx.close();
  }
}
