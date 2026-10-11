part of '../../aviary.dart';

/// A generated account: the signing secret, public key and chain address.
/// {@category Ledger}
abstract final class Account {
  /// Wraps the three byte strings of an account.
  const factory Account(
    Uint8List secret,
    Uint8List publicKey,
    Uint8List address,
  ) = _Account;

  /// The signing secret.
  Uint8List get secret;

  /// The public key.
  Uint8List get publicKey;

  /// The 32 byte chain address.
  Uint8List get address;
}
