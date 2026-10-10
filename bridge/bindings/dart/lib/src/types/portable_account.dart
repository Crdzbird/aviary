part of '../../blockchainer.dart';

/// Key material as returned by the portable account open methods, or a
/// hybrid KEM device key pair from [BlockchainerPortableSeal.hybridKemKeypair].
abstract final class PortableAccount {
  /// Wraps a public and secret half.
  const factory PortableAccount(Uint8List public, Uint8List secret) =
      _PortableAccount;

  /// The public half.
  Uint8List get public;

  /// The secret half.
  Uint8List get secret;
}
