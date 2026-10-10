part of '../../blockchainer.dart';

/// Raised when a native call reports an error.
abstract final class BlockchainerException implements Exception {
  /// An error carrying the native [message].
  const factory BlockchainerException(String message) = _BlockchainerException;

  /// The native error message.
  String get message;
}
