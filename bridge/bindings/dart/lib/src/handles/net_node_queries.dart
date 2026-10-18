part of '../../aviary.dart';

/// Read only views of a networked node's chain and mesh.
/// {@category Networking}
extension NetNodeQueries on NetNode {
  /// The committed chain height.
  int get height => _bc.lib
      .fn<_U64OfHandleC>('aviary_net_node_height')
      .asFunction<_U64OfHandleD>()(_h);

  /// The encoded tip header.
  Uint8List tip() => _buf('aviary_net_node_tip');

  /// The 32 byte state root.
  Uint8List stateRoot() => _buf('aviary_net_node_state_root');

  /// Every unique consensus payload of the current height, in arrival order.
  /// Replay it to a peer whose link connects after consensus started.
  List<Uint8List> replayLog() =>
      _splitBlobList(_buf('aviary_net_node_replay_log'));

  /// Whether [peer] has been banned for misbehaviour.
  bool peerBanned(int peer) =>
      _bc.lib
          .fn<_NetPeerFlagC>('aviary_net_node_peer_banned')
          .asFunction<_NetPeerFlagD>()(_h, peer) ==
      1;

  /// The balance of an address on this account model chain.
  int balance(Uint8List address) => _scoped(
    [address],
    (p) => _bc.lib
        .fn<_U64NodeAddrC>('aviary_net_node_balance')
        .asFunction<_U64NodeAddrD>()(_h, p[0]),
  );

  /// A state inclusion proof for an address.
  Uint8List proveAccount(Uint8List address) => _scoped(
    [address],
    (p) => _bc._take(
      _bc.lib
          .fn<_BufNodeAddrC>('aviary_net_node_prove_account')
          .asFunction<_BufNodeAddrD>()(_h, p[0]),
      'net_node_prove_account',
    ),
  );

  /// Record model networks: the committed record at [key], or null.
  RecordEntry? record(Uint8List key) {
    try {
      return _decodeRecord(
            _byKey('aviary_net_node_record', key),
            withKey: false,
          )
          as RecordEntry;
    } on AviaryException catch (e) {
      if (e.message.contains('no such record')) return null;
      rethrow;
    }
  }

  /// Record model networks: the next nonce [address] must use.
  int recordNonce(Uint8List address) => _scoped(
    [address],
    (p) => _bc.lib
        .fn<_RecAddrU64C>('aviary_net_node_record_nonce')
        .asFunction<_RecAddrU64D>()(_h, p[0]),
  );

  /// Record model networks: a state inclusion proof for the record at [key].
  Uint8List proveRecord(Uint8List key) =>
      _byKey('aviary_net_node_prove_record', key);

  Uint8List _byKey(String fn, Uint8List key) => _scoped(
    [key],
    (p) => _bc._take(
      _bc.lib.fn<_BufNodeAddrC>(fn).asFunction<_BufNodeAddrD>()(_h, p[0]),
      fn,
    ),
  );
}

Outbound _decodeOutbound(Uint8List b) {
  var off = 4;
  final relays = List<Relay>.generate(_u32be(b, 0), (_) {
    final at = off, len = _u32be(b, at + 4);
    off += 8 + len;
    return (to: _u32be(b, at), message: Uint8List.sublistView(b, at + 8, off));
  });
  final count = _u32be(b, off);
  final base = off + 4;
  final timeouts = List<TimeoutRequest>.generate(count, (i) {
    final at = base + i * 5;
    return (step: ConsensusStep.fromCode(b[at]), round: _u32be(b, at + 1));
  });
  return (relays: relays, timeouts: timeouts);
}
