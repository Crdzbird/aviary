part of '../../blockchainer.dart';

/// Driving every chain of a family together.
extension ChainTreeRounds on ChainTree {
  /// Produces one block on every chain, children before parents, so each
  /// parent anchors what its children just finalized. Returns the blocks in
  /// that order.
  List<Uint8List> produceRound(int timestamp) =>
      [for (final id in chainIds().reversed) produce(id, timestamp)];
}
