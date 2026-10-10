part of '../../../aviary.dart';

final class _BlockFinalized extends ChainEvent implements BlockFinalized {
  const _BlockFinalized({
    required this.height,
    required this.txCount,
    required this.blockId,
    required this.stateRoot,
  });

  @override
  String get type => 'block_finalized';

  @override
  final int height;

  @override
  final int txCount;

  @override
  final Uint8List blockId;

  @override
  final Uint8List stateRoot;

  @override
  String toString() =>
      'BlockFinalized(height: $height, txs: $txCount, id: ${blockId.shortHex})';
}
