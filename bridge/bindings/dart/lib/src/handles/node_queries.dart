part of '../../blockchainer.dart';

/// Read only views of a node.
extension NodeQueries on Node {
  int _u64(String fn) =>
      _lib.lookupFunction<_U64OfHandleC, _U64OfHandleD>(fn)(_h);

  int _u64Addr(String fn, Uint8List addr) => _scoped(
        [addr],
        (p) => _lib.lookupFunction<_U64NodeAddrC, _U64NodeAddrD>(fn)(_h, p[0]),
      );

  /// The chain height.
  int height() => _u64('blockchainer_node_height');

  /// Transactions waiting in the mempool.
  int mempoolSize() => _u64('blockchainer_node_mempool_size');

  /// The balance of an address.
  int balance(Uint8List address) =>
      _u64Addr('blockchainer_node_balance', address);

  /// The next nonce of an address.
  int nonce(Uint8List address) => _u64Addr('blockchainer_node_nonce', address);

  /// The 32 byte state root.
  Uint8List stateRoot() =>
      _bufOfNode('blockchainer_node_state_root', 'state_root');

  /// Chain events since the last drain, as a JSON array string.
  String drainEvents() =>
      utf8.decode(_bufOfNode('blockchainer_node_drain_events', 'drain_events'));

  /// A Prometheus format metrics snapshot.
  String metrics() =>
      utf8.decode(_bufOfNode('blockchainer_node_metrics', 'metrics'));

  /// A state inclusion proof for an address.
  Uint8List proveAccount(Uint8List address) => _scoped(
        [address],
        (p) => _bc._take(
          _lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>(
              'blockchainer_node_prove_account')(_h, p[0]),
          'prove_account',
        ),
      );
}
