part of '../../aviary.dart';

/// A validator or observer running BFT consensus over a gossip mesh, with no
/// socket and no clock of its own. Every input returns an [Outbound]: send
/// each relay to that peer over your transport, arm each timeout for a
/// duration you choose, and feed what arrives back through [onGossip] and
/// [onTimeout]. Blocks carry the transactions from [setTransactions] when
/// this node proposes.
/// {@category Networking}
final class NetNode extends _NativeHandle {
  NetNode._(Aviary bc, Pointer<Void> h) : super(bc, h, 'aviary_net_node_free');

  Uint8List _buf(String fn) => _bc._take(
    _bc.lib.fn<_BufOfHandleC>(fn).asFunction<_BufOfHandleD>()(_h),
    fn,
  );

  Outbound _outbound(_BufResult r, String op) =>
      _decodeOutbound(_bc._take(r, op));

  /// Adds a mesh neighbour.
  void addPeer(int peer) => _bc.lib
      .fn<_NetPeerC>('aviary_net_node_add_peer')
      .asFunction<_NetPeerD>()(_h, peer);

  /// The encoded transactions this node includes when it proposes at the
  /// current height; cleared when it commits, so set the next height's list
  /// before [advance]. Throws on a malformed transaction.
  void setTransactions(List<Uint8List> encodedTxs) {
    final blob = _blobList(encodedTxs);
    final ok = _scoped(
      [blob],
      (p) => _bc.lib
          .fn<_HandleBytesIntC>('aviary_net_node_set_transactions')
          .asFunction<_HandleBytesIntD>()(_h, p[0], blob.length),
    );
    if (ok != 1) {
      throw const AviaryException(
        'malformed transaction',
        AviaryStatus.badInput,
      );
    }
  }

  /// Begins consensus for the current height.
  Outbound start() => _outbound(
    _bc.lib
        .fn<_BufOfHandleC>('aviary_net_node_start')
        .asFunction<_BufOfHandleD>()(_h),
    'net_node_start',
  );

  /// Handles a gossip payload received from [from]. Duplicates and payloads
  /// from banned peers produce nothing; undecodable ones penalize the peer.
  Outbound onGossip(int from, Uint8List payload) => _scoped(
    [payload],
    (p) => _outbound(
      _bc.lib
          .fn<_NetGossipC>('aviary_net_node_on_gossip')
          .asFunction<_NetGossipD>()(_h, from, p[0], payload.length),
      'net_node_on_gossip',
    ),
  );

  /// Fires a timeout this node asked for.
  Outbound onTimeout(TimeoutRequest t) => _outbound(
    _bc.lib
        .fn<_NetTimeoutC>('aviary_net_node_on_timeout')
        .asFunction<_NetTimeoutD>()(_h, t.step.code, t.round),
    'net_node_on_timeout',
  );

  /// After [committed], rolls into the next height. Throws otherwise.
  Outbound advance() => _outbound(
    _bc.lib
        .fn<_BufOfHandleC>('aviary_net_node_advance')
        .asFunction<_BufOfHandleD>()(_h),
    'net_node_advance',
  );

  /// Whether the current height has committed.
  bool get committed =>
      _bc.lib
          .fn<_HandleFlagC>('aviary_net_node_committed')
          .asFunction<_HandleFlagD>()(_h) ==
      1;

  /// Releases the native node.
  void free() {
    _nodeSinks.remove(_h.address)?.close();
    _release();
  }
}
