part of '../../aviary.dart';

ChainEvent _chainEventFromJson(Map<String, Object?> json) {
  final type = json['type'];
  if (type is! String) return UnknownEvent('', json);
  switch (type) {
    case 'block_finalized':
      return BlockFinalized(
        height: _jsonInt(json, 'height'),
        txCount: _jsonInt(json, 'tx_count'),
        blockId: _jsonHex(json, 'block_id'),
        stateRoot: _jsonHex(json, 'state_root'),
      );
    case 'block_rejected':
      return BlockRejected(
        attemptedHeight: _jsonInt(json, 'attempted_height'),
        blockId: _jsonHex(json, 'block_id'),
      );
    case 'tx_admitted':
      return TxAdmitted(
        txid: _jsonHex(json, 'txid'),
        mempoolSize: _jsonInt(json, 'mempool_size'),
      );
    case 'tx_removed':
      return TxRemoved(
        txid: _jsonHex(json, 'txid'),
        mempoolSize: _jsonInt(json, 'mempool_size'),
      );
  }
  return UnknownEvent(type, json);
}

final Map<int, _NodeEventSink> _nodeSinks = {};

void _nodeEventTrampoline(Pointer<Void> ctx, Pointer<Utf8> json, int len) {
  final sink = _nodeSinks[ctx.address];
  if (sink == null) return;
  final text = utf8.decode(json.cast<Uint8>().asTypedList(len));
  final decoded = jsonDecode(text);
  if (decoded is Map<String, Object?>) {
    sink.controller.add(_chainEventFromJson(decoded));
  }
}

/// The native hook and broadcast controller behind one node's event stream.
final class _NodeEventSink {
  _NodeEventSink._(
    this._lib,
    this._node,
    this.controller,
    this._callable,
    this._symbol,
  );

  factory _NodeEventSink.attach(
    Aviary bc,
    Pointer<Void> node, [
    String symbol = 'aviary_node_set_event_hook',
  ]) {
    final callable = NativeCallable<_NodeEventC>.isolateLocal(
      _nodeEventTrampoline,
    )..keepIsolateAlive = false;
    bc.lib.fn<_NodeSetEventC>(symbol).asFunction<_NodeSetEventD>()(
      node,
      callable.nativeFunction,
      node,
    );
    final sink = _NodeEventSink._(
      bc.lib,
      node,
      StreamController<ChainEvent>.broadcast(),
      callable,
      symbol,
    );
    _nodeSinks[node.address] = sink;
    return sink;
  }

  final DynamicLibrary _lib;
  final Pointer<Void> _node;
  final StreamController<ChainEvent> controller;
  final NativeCallable<_NodeEventC> _callable;
  final String _symbol;

  /// Detaches the hook before the node is freed and closes the stream.
  void close() {
    _lib.fn<_NodeSetEventC>(_symbol).asFunction<_NodeSetEventD>()(
      _node,
      nullptr,
      nullptr,
    );
    _callable.close();
    _nodeSinks.remove(_node.address);
    unawaited(controller.close());
  }
}
