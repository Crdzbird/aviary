part of '../../../aviary.dart';

/// A transaction left the mempool, included in a block or evicted.
abstract final class TxRemoved extends ChainEvent {
  /// Describes a removed transaction.
  const factory TxRemoved({required Uint8List txid, required int mempoolSize}) =
      _TxRemoved;

  /// The 32 byte transaction id.
  Uint8List get txid;

  /// Transactions still waiting.
  int get mempoolSize;
}
