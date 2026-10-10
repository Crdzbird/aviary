part of '../../aviary.dart';

/// Raised when a native call reports an error.
abstract final class AviaryException implements Exception {
  /// An error carrying the native [message] and its [status]; calls that only
  /// report a message use [AviaryStatus.failure].
  const factory AviaryException(
    String message, [
    AviaryStatus status,
  ]) = _AviaryException;

  /// The native error message.
  String get message;

  /// The status code behind the error.
  AviaryStatus get status;
}
