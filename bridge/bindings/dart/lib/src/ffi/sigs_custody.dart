part of '../../aviary.dart';

typedef _KemKeygenC = _BufResult Function();
typedef _KemKeygenD = _BufResult Function();
typedef _PSealPassC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Utf8>, Uint64, Size);
typedef _PSealPassD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Utf8>, int, int);
typedef _POpenPassC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>);
typedef _POpenPassD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>);
typedef _PSealToC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealToD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _PSealSignedC = _BufResult Function(Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealSignedD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _POpenSignedC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _POpenSignedD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);

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

typedef _AsDescribeC = _BufResult Function(Uint16);
typedef _AsDescribeD = _BufResult Function(int);
typedef _TxEncodeC = _BufResult Function(Uint8, Uint64, Uint8, Uint64,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TxEncodeD = _BufResult Function(int, int, int, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _SmNewC = Pointer<Void> Function(Size);
typedef _SmNewD = Pointer<Void> Function(int);
typedef _SmAddC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _SmAddD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _SmSizeC = Size Function(Pointer<Void>);
typedef _SmSizeD = int Function(Pointer<Void>);
typedef _SmRevealC = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, Size, Size);
typedef _SmRevealD = _BufResult Function(
    Pointer<Void>, Pointer<Uint8>, int, int);

typedef _KsSignC = _BufResult Function(Pointer<Void>, Uint64, Pointer<Uint8>,
    Pointer<Uint8>, Uint64, Uint64, Uint64);
typedef _KsSignD = _BufResult Function(
    Pointer<Void>, int, Pointer<Uint8>, Pointer<Uint8>, int, int, int);
typedef _KsHasC = Int32 Function(Pointer<Void>, Pointer<Uint8>);
typedef _KsHasD = int Function(Pointer<Void>, Pointer<Uint8>);
typedef _SubmitKsC = _BufResult Function(Pointer<Void>, Pointer<Void>,
    Pointer<Uint8>, Pointer<Uint8>, Uint64, Uint64, Uint64);
typedef _SubmitKsD = _BufResult Function(Pointer<Void>, Pointer<Void>,
    Pointer<Uint8>, Pointer<Uint8>, int, int, int);
