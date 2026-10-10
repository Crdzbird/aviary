part of '../../aviary.dart';

final class _AviaryException implements AviaryException {
  const _AviaryException(this.message, [this.status = AviaryStatus.failure]);

  @override
  final String message;

  @override
  final AviaryStatus status;

  @override
  String toString() => status == AviaryStatus.failure
      ? 'AviaryException: $message'
      : 'AviaryException(${status.name}): $message';
}
