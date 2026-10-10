part of '../../../aviary.dart';

final class _TxRemoved extends ChainEvent implements TxRemoved {
  const _TxRemoved({required this.txid, required this.mempoolSize});

  @override
  String get type => 'tx_removed';

  @override
  final Uint8List txid;

  @override
  final int mempoolSize;

  @override
  String toString() =>
      'TxRemoved(txid: ${txid.shortHex}, mempool: $mempoolSize)';
}
