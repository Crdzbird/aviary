part of '../../blockchainer.dart';

final class _ThresholdShare implements ThresholdShare {
  const _ThresholdShare(
    this.bytes,
    this.index,
    this.groupPublicKey,
    this.publicShare,
  );

  @override
  final Uint8List bytes;

  @override
  final int index;

  @override
  final Uint8List groupPublicKey;

  @override
  final Uint8List publicShare;
}
