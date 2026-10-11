part of '../../../aviary.dart';

/// A block failed consensus or state validation.
/// {@category Observability}
abstract final class BlockRejected extends ChainEvent {
  /// Describes a rejected block.
  const factory BlockRejected({
    required int attemptedHeight,
    required Uint8List blockId,
  }) = _BlockRejected;

  /// The height the block claimed.
  int get attemptedHeight;

  /// The 32 byte block id.
  Uint8List get blockId;
}
