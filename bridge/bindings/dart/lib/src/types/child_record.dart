part of '../../aviary.dart';

/// One child's entry in a dynamic registry: what the parent commits about it
/// and what a child proof carries.
/// {@category Subchains and trees}
abstract final class ChildRecord {
  /// Wraps the fields of a decoded 230 byte record.
  const factory ChildRecord({
    required int index,
    required int chainId,
    required ChildStatus status,
    required int maxDepth,
    required Uint8List genesisHeaderHash,
    required int registeredHeight,
    required int closedHeight,
    required int tipHeight,
    required Uint8List tipHash,
    required Uint8List tipStateRoot,
    required Uint8List tipSubchainRoot,
    required Uint8List vsetHash,
    required Uint8List tipVsetHash,
    required Uint8List encoded,
  }) = _ChildRecord;

  /// The registration order, which is also the leaf position.
  int get index;

  /// The child's chain id.
  int get chainId;

  /// Whether the parent still anchors the child.
  ChildStatus get status;

  /// The child's own subtree depth, 0 for a leaf.
  int get maxDepth;

  /// The hash of the child's genesis header.
  Uint8List get genesisHeaderHash;

  /// The parent height that registered the child, 0 for the parent genesis.
  int get registeredHeight;

  /// The parent height that closed the child, 0 while open.
  int get closedHeight;

  /// The height of the last anchored child header.
  int get tipHeight;

  /// The hash of the last anchored child header.
  Uint8List get tipHash;

  /// The state root of the last anchored child header.
  Uint8List get tipStateRoot;

  /// The child's own registry root, zero unless it is a parent.
  Uint8List get tipSubchainRoot;

  /// The hash of the child's validator set in force for its next header.
  Uint8List get vsetHash;

  /// The hash of the validator set that certified the tip.
  Uint8List get tipVsetHash;

  /// The canonical 230 bytes.
  Uint8List get encoded;
}
