part of '../../aviary.dart';

typedef _IntVoid = Int32 Function();
typedef _PtrVoid = Pointer<Utf8> Function();
typedef _BufOfBytesC = _BufResult Function(Pointer<Uint8>, Size);
typedef _BufOfBytesD = _BufResult Function(Pointer<Uint8>, int);
typedef _BufOfPtrC = _BufResult Function(Pointer<Uint8>);
typedef _BufOfPtrD = _BufResult Function(Pointer<Uint8>);
typedef _Buf2C = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _Buf2D = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _VerifyC = Int32 Function(Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _VerifyD = int Function(Pointer<Uint8>, Pointer<Uint8>, int);
typedef _TxidC = _BufResult Function(Uint8, Uint64, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _TxidD = _BufResult Function(
    int, int, Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _HeaderC = _BufResult Function(Uint8, Uint64, Uint64, Pointer<Uint8>,
    Uint64, Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, Size);
typedef _HeaderD = _BufResult Function(int, int, int, Pointer<Uint8>, int,
    Pointer<Uint8>, Pointer<Uint8>, Pointer<Uint8>, int);
typedef _VerifyCommitC = Int32 Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _VerifyCommitD = int Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _Erc20C = _BufResult Function(Pointer<Uint8>, Uint64);
typedef _Erc20D = _BufResult Function(Pointer<Uint8>, int);
typedef _SelectorC = _BufResult Function(Pointer<Utf8>);
typedef _SelectorD = _BufResult Function(Pointer<Utf8>);
typedef _SignLegacyC = _BufResult Function(
    Uint64,
    Uint64,
    Uint64,
    Pointer<Uint8>,
    Int32,
    Uint64,
    Pointer<Uint8>,
    Size,
    Uint64,
    Pointer<Uint8>);
typedef _SignLegacyD = _BufResult Function(int, int, int, Pointer<Uint8>, int,
    int, Pointer<Uint8>, int, int, Pointer<Uint8>);
typedef _BufFreeC = Void Function(Pointer<_Buffer>);
typedef _BufFreeD = void Function(Pointer<_Buffer>);
typedef _StrFreeC = Void Function(Pointer<Utf8>);
typedef _StrFreeD = void Function(Pointer<Utf8>);
typedef _HandleFreeC = Void Function(Pointer<Void>);
typedef _HandleFreeD = void Function(Pointer<Void>);
typedef _BufOfHandleC = _BufResult Function(Pointer<Void>);
typedef _BufOfHandleD = _BufResult Function(Pointer<Void>);
typedef _U64OfHandleC = Uint64 Function(Pointer<Void>);
typedef _U64OfHandleD = int Function(Pointer<Void>);
typedef _IntOfHandleC = Int32 Function(Pointer<Void>);
typedef _IntOfHandleD = int Function(Pointer<Void>);
