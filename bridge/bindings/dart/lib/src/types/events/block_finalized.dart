part of '../../../aviary.dart';

/// A block was finalized and applied.
abstract final class BlockFinalized extends ChainEvent {
  /// Describes a finalized block.
  const factory BlockFinalized({
    required int height,
    required int txCount,
    required Uint8List blockId,
    required Uint8List stateRoot,
  }) = _BlockFinalized;

  /// The new chain height.
  int get height;

  /// Transactions the block carried.
  int get txCount;

  /// The 32 byte block id.
  Uint8List get blockId;

  /// The 32 byte state root after the block.
  Uint8List get stateRoot;
}
