part of '../../../aviary.dart';

final class _BlockRejected extends ChainEvent implements BlockRejected {
  const _BlockRejected({required this.attemptedHeight, required this.blockId});

  @override
  String get type => 'block_rejected';

  @override
  final int attemptedHeight;

  @override
  final Uint8List blockId;

  @override
  String toString() =>
      'BlockRejected(height: $attemptedHeight, id: ${blockId.shortHex})';
}
