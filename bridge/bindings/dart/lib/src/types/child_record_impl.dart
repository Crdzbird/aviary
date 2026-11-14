part of '../common.dart';

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

/// Parses the fixed 230 byte record layout. Integers are read as two 32 bit
/// halves, which compiles to the web as well as to native code.
ChildRecord childRecordOf(Uint8List b) {
  if (b.length != _childRecordSize) {
    throw const AviaryException('child record: must be 230 bytes');
  }
  final bd = ByteData.sublistView(b);
  Uint8List cut(int from) => Uint8List.fromList(b.sublist(from, from + 32));
  int u64(int at) => bd.getUint32(at) * 0x100000000 + bd.getUint32(at + 4);
  return ChildRecord(
    index: bd.getUint32(0),
    chainId: u64(4),
    status: ChildStatus.fromCode(b[12]),
    maxDepth: b[13],
    genesisHeaderHash: cut(14),
    registeredHeight: u64(46),
    closedHeight: u64(54),
    tipHeight: u64(62),
    tipHash: cut(70),
    tipStateRoot: cut(102),
    tipSubchainRoot: cut(134),
    vsetHash: cut(166),
    tipVsetHash: cut(198),
    encoded: Uint8List.fromList(b),
  );
}
