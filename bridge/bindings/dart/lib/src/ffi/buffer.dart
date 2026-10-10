part of '../../aviary.dart';

final class _Buffer extends Struct {
  external Pointer<Uint8> data;
  @Size()
  external int len;
}
