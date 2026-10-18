part of '../../aviary.dart';

/// Raised when a native call reports an error.
/// {@category Errors and helpers}
abstract final class AviaryException implements Exception {
  /// An error carrying the native [message] and its [status]; calls that only
  /// report a message use [AviaryStatus.failure].
  const factory AviaryException(String message, [AviaryStatus status]) =
      _AviaryException;

  /// The native error message.
  String get message;

  /// The status code behind the error.
  AviaryStatus get status;
}

/// The stable status codes of the C ABI. Values never change.
/// {@category Errors and helpers}
enum AviaryStatus {
  /// Success.
  ok(0),

  /// A failure with no more specific mapping.
  failure(1),

  /// Genesis is sealed because a block was produced; fund accounts with
  /// transfers instead of credits.
  genesisSealed(2),

  /// Malformed input: a wrong length, a missing value or a bad encoding.
  badInput(3);

  const AviaryStatus(this.code);

  /// The ABI value.
  final int code;

  /// The status for an ABI [code]; unknown codes map to [failure].
  static AviaryStatus fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => failure);
}
