part of '../../aviary.dart';

final class _Account implements Account {
  const _Account(this.secret, this.publicKey, this.address);

  @override
  final Uint8List secret;

  @override
  final Uint8List publicKey;

  @override
  final Uint8List address;
}
