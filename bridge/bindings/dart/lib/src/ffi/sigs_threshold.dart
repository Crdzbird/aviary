part of '../../blockchainer.dart';

typedef _ThCreateC = _BufResult Function(Uint16, Uint16);
typedef _ThCreateD = _BufResult Function(int, int);
typedef _ThSealC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Utf8>, Uint64, Size);
typedef _ThSealD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Utf8>, int, int);
typedef _ThUnsealC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>);
typedef _ThUnsealD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>);
typedef _ThCerNewC = Pointer<Void> Function(
    Pointer<Uint8>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ThCerNewD = Pointer<Void> Function(
    Pointer<Uint8>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ThSignC = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>);
typedef _ThSignD = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>);
typedef _ThVerifyC = Int32 Function(
    Pointer<Void>, Uint16, Pointer<Uint8>, Pointer<Uint8>);
typedef _ThVerifyD = int Function(
    Pointer<Void>, int, Pointer<Uint8>, Pointer<Uint8>);
typedef _ThAggC = _BufResult Function(
    Pointer<Void>, Pointer<Uint16>, Pointer<Uint8>, Size);
typedef _ThAggD = _BufResult Function(
    Pointer<Void>, Pointer<Uint16>, Pointer<Uint8>, int);
