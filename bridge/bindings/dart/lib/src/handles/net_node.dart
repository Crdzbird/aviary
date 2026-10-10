part of '../../aviary.dart';

Outbound _decodeOutbound(Uint8List b) {
  var off = 0;
  final relayCount = _u32be(b, off);
  off += 4;
  final relays = <Relay>[];
  for (var i = 0; i < relayCount; i++) {
    final to = _u32be(b, off);
    final len = _u32be(b, off + 4);
    relays.add(
        (to: to, message: Uint8List.sublistView(b, off + 8, off + 8 + len)));
    off += 8 + len;
  }
  final timeoutCount = _u32be(b, off);
  off += 4;
  final timeouts = <TimeoutRequest>[];
  for (var i = 0; i < timeoutCount; i++) {
    timeouts
        .add((step: ConsensusStep.fromCode(b[off]), round: _u32be(b, off + 1)));
    off += 5;
  }
  return (relays: relays, timeouts: timeouts);
}

/// A validator or observer running BFT consensus over a gossip mesh, with no
/// socket and no clock of its own. Every input returns an [Outbound]: send
/// each relay to that peer over your transport, arm each timeout for a
/// duration you choose, and feed what arrives back through [onGossip] and
/// [onTimeout]. Blocks carry the transactions from [setTransactions] when
/// this node proposes.
final class NetNode extends _NativeHandle {
  NetNode._(Aviary bc, Pointer<Void> h) : super(bc, h, 'aviary_net_node_free');

  Outbound _outbound(_BufResult r, String op) =>
      _decodeOutbound(_bc._take(r, op));

  /// Adds a mesh neighbour.
  void addPeer(int peer) =>
      _bc.lib.lookupFunction<_NetPeerC, _NetPeerD>('aviary_net_node_add_peer')(
          _h, peer);

  /// The encoded transactions this node includes when it proposes at the
  /// current height; cleared by [advance]. Throws on a malformed transaction.
  void setTransactions(List<Uint8List> encodedTxs) {
    final blob = _blobList(encodedTxs);
    final ok = _scoped(
      [blob],
      (p) => _bc.lib.lookupFunction<_HandleBytesIntC, _HandleBytesIntD>(
          'aviary_net_node_set_transactions')(_h, p[0], blob.length),
    );
    if (ok != 1) {
      throw const AviaryException(
          'malformed transaction', AviaryStatus.badInput);
    }
  }

  /// Begins consensus for the current height.
  Outbound start() => _outbound(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(
          'aviary_net_node_start')(_h),
      'net_node_start');

  /// Handles a gossip payload received from [from]. Duplicates and payloads
  /// from banned peers produce nothing; undecodable ones penalize the peer.
  Outbound onGossip(int from, Uint8List payload) => _scoped(
        [payload],
        (p) => _outbound(
          _bc.lib.lookupFunction<_NetGossipC, _NetGossipD>(
              'aviary_net_node_on_gossip')(_h, from, p[0], payload.length),
          'net_node_on_gossip',
        ),
      );

  /// Fires a timeout this node asked for.
  Outbound onTimeout(TimeoutRequest t) => _outbound(
      _bc.lib.lookupFunction<_NetTimeoutC, _NetTimeoutD>(
          'aviary_net_node_on_timeout')(_h, t.step.code, t.round),
      'net_node_on_timeout');

  /// After [committed], rolls into the next height. Throws otherwise.
  Outbound advance() => _outbound(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(
          'aviary_net_node_advance')(_h),
      'net_node_advance');

  /// Whether the current height has committed.
  bool get committed =>
      _bc.lib.lookupFunction<_HandleFlagC, _HandleFlagD>(
          'aviary_net_node_committed')(_h) ==
      1;

  /// Releases the native node.
  void free() => _release();
}
