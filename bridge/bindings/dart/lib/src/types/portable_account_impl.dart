part of '../../blockchainer.dart';

final class _PortableAccount implements PortableAccount {
  const _PortableAccount(this.public, this.secret);

  @override
  final Uint8List public;

  @override
  final Uint8List secret;
}
