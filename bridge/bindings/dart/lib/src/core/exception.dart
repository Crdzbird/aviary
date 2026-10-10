part of '../../aviary.dart';

/// Raised when a native call reports an error.
abstract final class AviaryException implements Exception {
  /// An error carrying the native [message].
  const factory AviaryException(String message) = _AviaryException;

  /// The native error message.
  String get message;
}
