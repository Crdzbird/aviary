part of '../../../aviary.dart';

/// A transaction entered the mempool.
abstract final class TxAdmitted extends ChainEvent {
  /// Describes an admitted transaction.
  const factory TxAdmitted({
    required Uint8List txid,
    required int mempoolSize,
  }) = _TxAdmitted;

  /// The 32 byte transaction id.
  Uint8List get txid;

  /// Transactions waiting after this one was admitted.
  int get mempoolSize;
}
