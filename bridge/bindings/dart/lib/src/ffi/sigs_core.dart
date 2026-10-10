part of '../../aviary.dart';

typedef _U8 = Pointer<Uint8>;
typedef _V = Pointer<Void>;
typedef _BR = _BufResult;

typedef _IntVoid = Int32 Function();
typedef _PtrVoid = Pointer<Utf8> Function();
typedef _BufOfBytesC = _BR Function(_U8, Size);
typedef _BufOfBytesD = _BR Function(_U8, int);
typedef _BufOfPtrC = _BR Function(_U8);
typedef _BufOfPtrD = _BR Function(_U8);
typedef _Buf2C = _BR Function(_U8, Size, _U8, Size);
typedef _Buf2D = _BR Function(_U8, int, _U8, int);
typedef _VerifyC = Int32 Function(_U8, _U8, Size);
typedef _VerifyD = int Function(_U8, _U8, int);
typedef _TxidC = _BR Function(Uint8, Uint64, _U8, Size, _U8, Size, _U8, Size);
typedef _TxidD = _BR Function(int, int, _U8, int, _U8, int, _U8, int);
typedef _HeaderC =
    _BR Function(Uint8, Uint64, Uint64, _U8, Uint64, _U8, _U8, _U8, Size);
typedef _HeaderD = _BR Function(int, int, int, _U8, int, _U8, _U8, _U8, int);
typedef _VerifyCommitC = Int32 Function(Uint64, _U8, Size, _U8, Size);
typedef _VerifyCommitD = int Function(int, _U8, int, _U8, int);
typedef _Erc20C = _BR Function(_U8, Uint64);
typedef _Erc20D = _BR Function(_U8, int);
typedef _SelectorC = _BR Function(Pointer<Utf8>);
typedef _SelectorD = _BR Function(Pointer<Utf8>);
typedef _SignLegacyC =
    _BR Function(
      Uint64,
      Uint64,
      Uint64,
      _U8,
      Int32,
      Uint64,
      _U8,
      Size,
      Uint64,
      _U8,
    );
typedef _SignLegacyD =
    _BR Function(int, int, int, _U8, int, int, _U8, int, int, _U8);
typedef _BufFreeC = Void Function(Pointer<_Buffer>);
typedef _BufFreeD = void Function(Pointer<_Buffer>);
typedef _StrFreeC = Void Function(Pointer<Utf8>);
typedef _StrFreeD = void Function(Pointer<Utf8>);
typedef _HandleFreeC = Void Function(_V);
typedef _HandleFreeD = void Function(_V);
typedef _BufOfHandleC = _BR Function(_V);
typedef _BufOfHandleD = _BR Function(_V);
typedef _U64OfHandleC = Uint64 Function(_V);
typedef _U64OfHandleD = int Function(_V);
typedef _IntOfHandleC = Int32 Function(_V);
typedef _IntOfHandleD = int Function(_V);

typedef _NodeNewC = _V Function(Uint64);
typedef _NodeNewD = _V Function(int);
typedef _CreditC = Int32 Function(_V, _U8, Uint64);
typedef _CreditD = int Function(_V, _U8, int);
typedef _SubmitC = _BR Function(_V, _U8, Size, _U8, Uint64, Uint64);
typedef _SubmitD = _BR Function(_V, _U8, int, _U8, int, int);
typedef _SubmitFeeC = _BR Function(_V, _U8, Size, _U8, Uint64, Uint64, Uint64);
typedef _SubmitFeeD = _BR Function(_V, _U8, int, _U8, int, int, int);
typedef _SubmitHybridC =
    _BR Function(_V, _U8, Size, _U8, Size, _U8, Uint64, Uint64);
typedef _SubmitHybridD = _BR Function(_V, _U8, int, _U8, int, _U8, int, int);
typedef _ProduceC = Int32 Function(_V, Uint64, Pointer<Uint64>);
typedef _ProduceD = int Function(_V, int, Pointer<Uint64>);
typedef _U64NodeAddrC = Uint64 Function(_V, _U8);
typedef _U64NodeAddrD = int Function(_V, _U8);
typedef _BufNodeAddrC = _BR Function(_V, _U8);
typedef _BufNodeAddrD = _BR Function(_V, _U8);
typedef _KsNewC = _V Function(Pointer<Utf8>);
typedef _KsNewD = _V Function(Pointer<Utf8>);
typedef _KsAddC = Int32 Function(_V, _U8, Size, _U8, Size, _U8);
typedef _KsAddD = int Function(_V, _U8, int, _U8, int, _U8);
typedef _KsLoadC = _V Function(_U8, Size);
typedef _KsLoadD = _V Function(_U8, int);
typedef _KsUnlockC = Int32 Function(_V, Pointer<Utf8>);
typedef _KsUnlockD = int Function(_V, Pointer<Utf8>);

typedef _NodeEventC = Void Function(_V, Pointer<Utf8>, Size);
typedef _NodeSetEventC =
    Void Function(_V, Pointer<NativeFunction<_NodeEventC>>, _V);
typedef _NodeSetEventD =
    void Function(_V, Pointer<NativeFunction<_NodeEventC>>, _V);
