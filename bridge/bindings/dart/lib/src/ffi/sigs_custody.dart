part of '../../aviary.dart';

typedef _KemKeygenC = _BR Function();
typedef _KemKeygenD = _BR Function();
typedef _PSealPassC =
    _BR Function(_U8, Size, _U8, Size, Pointer<Utf8>, Uint64, Size);
typedef _PSealPassD = _BR Function(_U8, int, _U8, int, Pointer<Utf8>, int, int);
typedef _POpenPassC = _BR Function(_U8, Size, Pointer<Utf8>);
typedef _POpenPassD = _BR Function(_U8, int, Pointer<Utf8>);
typedef _PSealToC = _BR Function(_U8, Size, _U8, Size, _U8, Size);
typedef _PSealToD = _BR Function(_U8, int, _U8, int, _U8, int);
typedef _PSealSignedC =
    _BR Function(_U8, Size, _U8, Size, _U8, Size, _U8, Size);
typedef _PSealSignedD = _BR Function(_U8, int, _U8, int, _U8, int, _U8, int);
typedef _POpenSignedC = _BR Function(_U8, Size, _U8, Size, _U8, Size);
typedef _POpenSignedD = _BR Function(_U8, int, _U8, int, _U8, int);

typedef _ThCreateC = _BR Function(Uint16, Uint16);
typedef _ThCreateD = _BR Function(int, int);
typedef _ThSealC = _BR Function(_U8, Size, Pointer<Utf8>, Uint64, Size);
typedef _ThSealD = _BR Function(_U8, int, Pointer<Utf8>, int, int);
typedef _ThUnsealC = _BR Function(_U8, Size, Pointer<Utf8>);
typedef _ThUnsealD = _BR Function(_U8, int, Pointer<Utf8>);
typedef _ThCerNewC = _V Function(_U8, _U8, Size, _U8, Size);
typedef _ThCerNewD = _V Function(_U8, _U8, int, _U8, int);
typedef _ThSignC = _BR Function(_V, _U8, Size, _U8);
typedef _ThSignD = _BR Function(_V, _U8, int, _U8);
typedef _ThVerifyC = Int32 Function(_V, Uint16, _U8, _U8);
typedef _ThVerifyD = int Function(_V, int, _U8, _U8);
typedef _ThAggC = _BR Function(_V, Pointer<Uint16>, _U8, Size);
typedef _ThAggD = _BR Function(_V, Pointer<Uint16>, _U8, int);

typedef _AsDescribeC = _BR Function(Uint16);
typedef _AsDescribeD = _BR Function(int);
typedef _TxEncodeC =
    _BR Function(Uint8, Uint64, Uint8, Uint64, _U8, Size, _U8, Size, _U8, Size);
typedef _TxEncodeD =
    _BR Function(int, int, int, int, _U8, int, _U8, int, _U8, int);
typedef _SmNewC = _V Function(Size);
typedef _SmNewD = _V Function(int);
typedef _SmAddC = Int32 Function(_V, _U8, Size);
typedef _SmAddD = int Function(_V, _U8, int);
typedef _SmSizeC = Size Function(_V);
typedef _SmSizeD = int Function(_V);
typedef _SmRevealC = _BR Function(_V, _U8, Size, Size);
typedef _SmRevealD = _BR Function(_V, _U8, int, int);

typedef _KsSignC = _BR Function(_V, Uint64, _U8, _U8, Uint64, Uint64, Uint64);
typedef _KsSignD = _BR Function(_V, int, _U8, _U8, int, int, int);
typedef _KsHasC = Int32 Function(_V, _U8);
typedef _KsHasD = int Function(_V, _U8);
typedef _SubmitKsC = _BR Function(_V, _V, _U8, _U8, Uint64, Uint64, Uint64);
typedef _SubmitKsD = _BR Function(_V, _V, _U8, _U8, int, int, int);
