part of '../../blockchainer.dart';

final class _BlockchainerException implements BlockchainerException {
  const _BlockchainerException(this.message);

  @override
  final String message;

  @override
  String toString() => 'BlockchainerException: $message';
}
