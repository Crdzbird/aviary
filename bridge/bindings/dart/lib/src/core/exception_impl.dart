part of '../../aviary.dart';

final class _AviaryException implements AviaryException {
  const _AviaryException(this.message);

  @override
  final String message;

  @override
  String toString() => 'AviaryException: $message';
}
