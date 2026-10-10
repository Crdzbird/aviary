part of '../../aviary.dart';

/// Read only views of a node.
extension NodeQueries on Node {
  int _u64(String fn) =>
      _lib.lookupFunction<_U64OfHandleC, _U64OfHandleD>(fn)(_h);

  int _u64Addr(String fn, Uint8List addr) => _scoped([
    addr,
  ], (p) => _lib.lookupFunction<_U64NodeAddrC, _U64NodeAddrD>(fn)(_h, p[0]));

  /// The chain height.
  int height() => _u64('aviary_node_height');

  /// Transactions waiting in the mempool.
  int mempoolSize() => _u64('aviary_node_mempool_size');

  /// The balance of an address.
  int balance(Uint8List address) => _u64Addr('aviary_node_balance', address);

  /// The next nonce of an address.
  int nonce(Uint8List address) => _u64Addr('aviary_node_nonce', address);

  /// The 32 byte state root.
  Uint8List stateRoot() => _bufOfNode('aviary_node_state_root', 'state_root');

  /// Chain events since the last drain, as a JSON array string.
  String drainEvents() =>
      utf8.decode(_bufOfNode('aviary_node_drain_events', 'drain_events'));

  /// A Prometheus format metrics snapshot.
  String metrics() => utf8.decode(_bufOfNode('aviary_node_metrics', 'metrics'));

  /// A state inclusion proof for an address.
  Uint8List proveAccount(Uint8List address) => _scoped(
    [address],
    (p) => _bc._take(
      _lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>(
        'aviary_node_prove_account',
      )(_h, p[0]),
      'prove_account',
    ),
  );
}

/// Typed chain events, pulled or pushed.
extension NodeEvents on Node {
  /// Chain events buffered since the last call, decoded. Returns nothing
  /// while [eventStream] is live, because events then go to the stream.
  List<ChainEvent> events() => _chainEventsFromJsonArray(drainEvents());

  /// Every chain event as it happens. The engine hands each event over
  /// synchronously inside the node call that caused it; listeners run on the
  /// next microtask, after that call has returned, so a listener may call the
  /// node again. Listening redirects events from the buffer behind [events]
  /// to this broadcast stream until the node is disposed, which closes it.
  Stream<ChainEvent> get eventStream =>
      (_nodeSinks[_h.address] ??= _NodeEventSink.attach(
        _bc,
        _h,
      )).controller.stream;
}

List<ChainEvent> _chainEventsFromJsonArray(String text) {
  final decoded = jsonDecode(text);
  if (decoded is! List) return const [];
  return [
    for (final e in decoded)
      if (e is Map<String, Object?>) _chainEventFromJson(e),
  ];
}
