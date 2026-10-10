part of '../../blockchainer.dart';

/// One participant's share of a threshold account. Persist [bytes] sealed.
abstract final class ThresholdShare {
  /// Wraps a share card and the facts read from it.
  const factory ThresholdShare(
    Uint8List bytes,
    int index,
    Uint8List groupPublicKey,
    Uint8List publicShare,
  ) = _ThresholdShare;

  /// The share card.
  Uint8List get bytes;

  /// The participant index.
  int get index;

  /// The account's on chain author key.
  Uint8List get groupPublicKey;

  /// This participant's public share.
  Uint8List get publicShare;
}
