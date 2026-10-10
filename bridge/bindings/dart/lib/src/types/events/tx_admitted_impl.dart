part of '../../../aviary.dart';

final class _TxAdmitted extends ChainEvent implements TxAdmitted {
  const _TxAdmitted({required this.txid, required this.mempoolSize});

  @override
  String get type => 'tx_admitted';

  @override
  final Uint8List txid;

  @override
  final int mempoolSize;

  @override
  String toString() =>
      'TxAdmitted(txid: ${txid.shortHex}, mempool: $mempoolSize)';
}
