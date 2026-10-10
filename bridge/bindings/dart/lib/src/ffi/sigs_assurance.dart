part of '../../blockchainer.dart';

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
