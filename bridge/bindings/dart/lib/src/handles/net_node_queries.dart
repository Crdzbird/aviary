part of '../../aviary.dart';

/// Read only views of a networked node's chain and mesh.
extension NetNodeQueries on NetNode {
  /// The committed chain height.
  int get height => _bc.lib.lookupFunction<_U64OfHandleC, _U64OfHandleD>(
      'aviary_net_node_height')(_h);

  Uint8List _buf(String fn) => _bc._take(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(fn)(_h), fn);

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
      _bc.lib.lookupFunction<_NetPeerFlagC, _NetPeerFlagD>(
          'aviary_net_node_peer_banned')(_h, peer) ==
      1;

  /// The balance of an address on this account model chain.
  int balance(Uint8List address) => _scoped(
        [address],
        (p) => _bc.lib.lookupFunction<_U64NodeAddrC, _U64NodeAddrD>(
            'aviary_net_node_balance')(_h, p[0]),
      );

  /// A state inclusion proof for an address.
  Uint8List proveAccount(Uint8List address) => _scoped(
        [address],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>(
              'aviary_net_node_prove_account')(_h, p[0]),
          'net_node_prove_account',
        ),
      );
}
