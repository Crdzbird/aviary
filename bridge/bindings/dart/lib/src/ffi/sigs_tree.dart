part of '../../aviary.dart';

typedef _TreeNodeC = _BufResult Function(
    Uint64,
    Pointer<Uint8>,
    Size,
    Uint16,
    Pointer<Uint8>,
    Pointer<Uint64>,
    Size,
    Pointer<Uint8>,
    Pointer<Uint8>,
    Pointer<Uint64>,
    Size,
    Uint16,
    Uint16,
    Pointer<Uint8>,
    Size);
typedef _TreeNodeD = _BufResult Function(
    int,
    Pointer<Uint8>,
    int,
    int,
    Pointer<Uint8>,
    Pointer<Uint64>,
    int,
    Pointer<Uint8>,
    Pointer<Uint8>,
    Pointer<Uint64>,
    int,
    int,
    int,
    Pointer<Uint8>,
    int);
typedef _TreeBuildC = Pointer<Void> Function(Pointer<Uint8>, Size);
typedef _TreeBuildD = Pointer<Void> Function(Pointer<Uint8>, int);
typedef _TreeVerifyC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _TreeVerifyD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _TreeU32IdC = Uint32 Function(Pointer<Void>, Uint64);
typedef _TreeU32IdD = int Function(Pointer<Void>, int);
typedef _TreeU64IdC = Uint64 Function(Pointer<Void>, Uint64);
typedef _TreeU64IdD = int Function(Pointer<Void>, int);
typedef _TreeProduceC = _BufResult Function(Pointer<Void>, Uint64, Uint64);
typedef _TreeProduceD = _BufResult Function(Pointer<Void>, int, int);
typedef _TreeProveC = _BufResult Function(
    Pointer<Void>, Uint64, Pointer<Uint8>);
typedef _TreeProveD = _BufResult Function(Pointer<Void>, int, Pointer<Uint8>);
typedef _TreeFinC = Void Function(Pointer<Void>, Uint64, Pointer<Uint8>, Size);
typedef _TreeTxC = Int32 Function(
    Pointer<Void>, Uint64, Uint64, Pointer<Uint8>, Size, Pointer<Size>);
typedef _TreeSetFinC = Void Function(
    Pointer<Void>, Pointer<NativeFunction<_TreeFinC>>, Pointer<Void>);
typedef _TreeSetFinD = void Function(
    Pointer<Void>, Pointer<NativeFunction<_TreeFinC>>, Pointer<Void>);
typedef _TreeSetTxC = Void Function(
    Pointer<Void>, Pointer<NativeFunction<_TreeTxC>>, Pointer<Void>);
typedef _TreeSetTxD = void Function(
    Pointer<Void>, Pointer<NativeFunction<_TreeTxC>>, Pointer<Void>);
