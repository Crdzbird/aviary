part of '../../blockchainer.dart';

/// A built family of chains driven in process. [produce] anchors children
/// upward automatically; [ChainTreeQueries.proveAccount] proves any
/// descendant from the root header.
final class ChainTree {
  ChainTree._(this._bc, this._h) : _handle = _nextTreeHandle++ {
    _trees[_handle] = this;
    _fin = NativeCallable<_TreeFinC>.isolateLocal(_treeFinTrampoline)
      ..keepIsolateAlive = false;
    _tx = NativeCallable<_TreeTxC>.isolateLocal(_treeTxTrampoline,
        exceptionalReturn: 0)
      ..keepIsolateAlive = false;
    final ctx = Pointer<Void>.fromAddress(_handle);
    _bc.lib.lookupFunction<_TreeSetFinC, _TreeSetFinD>(
            'blockchainer_chain_tree_set_finalized_hook')(
        _h, _fin.nativeFunction, ctx);
    _bc.lib.lookupFunction<_TreeSetTxC, _TreeSetTxD>(
        'blockchainer_chain_tree_set_tx_source')(_h, _tx.nativeFunction, ctx);
  }

  final Blockchainer _bc;
  final Pointer<Void> _h;
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
            'blockchainer_chain_tree_produce')(_h, chainId, timestamp),
        'chain_tree_produce',
      );

  /// Whether [proof] is committed by the root's current header.
  bool verify(Uint8List proof) => _scoped(
        [proof],
        (p) =>
            _bc.lib.lookupFunction<_TreeVerifyC, _TreeVerifyD>(
                'blockchainer_chain_tree_verify')(_h, p[0], proof.length) ==
            1,
      );

  /// Releases the native tree and its hooks.
  void free() {
    _bc.lib.lookupFunction<_HandleFreeC, _HandleFreeD>(
        'blockchainer_chain_tree_free')(_h);
    _trees.remove(_handle);
    _fin.close();
    _tx.close();
  }
}
