part of '../../aviary.dart';

final class _ChildRecord implements ChildRecord {
  const _ChildRecord({
    required this.index,
    required this.chainId,
    required this.status,
    required this.maxDepth,
    required this.genesisHeaderHash,
    required this.registeredHeight,
    required this.closedHeight,
    required this.tipHeight,
    required this.tipHash,
    required this.tipStateRoot,
    required this.tipSubchainRoot,
    required this.vsetHash,
    required this.tipVsetHash,
    required this.encoded,
  });

  @override
  final int index;
  @override
  final int chainId;
  @override
  final ChildStatus status;
  @override
  final int maxDepth;
  @override
  final Uint8List genesisHeaderHash;
  @override
  final int registeredHeight;
  @override
  final int closedHeight;
  @override
  final int tipHeight;
  @override
  final Uint8List tipHash;
  @override
  final Uint8List tipStateRoot;
  @override
  final Uint8List tipSubchainRoot;
  @override
  final Uint8List vsetHash;
  @override
  final Uint8List tipVsetHash;
  @override
  final Uint8List encoded;
}

const _childRecordSize = 230;

ChildRecord _childRecordOf(Uint8List b) {
  if (b.length != _childRecordSize) {
    throw const AviaryException('child record: must be 230 bytes');
  }
  final bd = ByteData.sublistView(b);
  Uint8List cut(int from) => Uint8List.fromList(b.sublist(from, from + 32));
  return ChildRecord(
    index: bd.getUint32(0),
    chainId: bd.getUint64(4),
    status: ChildStatus.fromCode(b[12]),
    maxDepth: b[13],
    genesisHeaderHash: cut(14),
    registeredHeight: bd.getUint64(46),
    closedHeight: bd.getUint64(54),
    tipHeight: bd.getUint64(62),
    tipHash: cut(70),
    tipStateRoot: cut(102),
    tipSubchainRoot: cut(134),
    vsetHash: cut(166),
    tipVsetHash: cut(198),
    encoded: Uint8List.fromList(b),
  );
}
