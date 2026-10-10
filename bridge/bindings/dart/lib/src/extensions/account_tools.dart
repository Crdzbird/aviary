part of '../../blockchainer.dart';

/// Convenience views of an account.
extension AccountTools on Account {
  /// The chain address in lowercase hex.
  String get addressHex => address.hex;

  /// Whether this is a hybrid post quantum account. Its secret is longer
  /// than the 64 bytes of an Ed25519 secret.
  bool get isHybrid => secret.length != 64;
}
