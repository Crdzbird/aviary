part of '../../aviary.dart';

typedef _RecSignHybC =
    _BR Function(Uint64, Uint64, _U8, Uint8, _U8, Size, _U8, Size, _U8, Size);
typedef _RecSignHybD =
    _BR Function(int, int, _U8, int, _U8, int, _U8, int, _U8, int);
typedef _KsSignRecC =
    _BR Function(_V, Uint64, _U8, Uint64, _U8, Uint8, _U8, Size);
typedef _KsSignRecD = _BR Function(_V, int, _U8, int, _U8, int, _U8, int);
typedef _PeerSignC =
    _BR Function(
      Uint32,
      Pointer<Utf8>,
      Uint16,
      _U8,
      Size,
      _U8,
      Size,
      _U8,
      Size,
      _U8,
      Size,
    );
typedef _PeerSignD =
    _BR Function(
      int,
      Pointer<Utf8>,
      int,
      _U8,
      int,
      _U8,
      int,
      _U8,
      int,
      _U8,
      int,
    );
typedef _BytesIntC = Int32 Function(_U8, Size);
typedef _BytesIntD = int Function(_U8, int);
typedef _VNoArgC = _V Function();
typedef _VOfBytesC = _V Function(_U8, Size);
typedef _VOfBytesD = _V Function(_U8, int);
typedef _BookMergeC = Int64 Function(_V, _U8, Size);
typedef _BookMergeD = int Function(_V, _U8, int);
typedef _BookIdBufC = _BR Function(_V, Uint32);
typedef _BookIdBufD = _BR Function(_V, int);
typedef _BookIdIntC = Int32 Function(_V, Uint32);
typedef _BookIdIntD = int Function(_V, int);
typedef _NoiseInitC = _V Function(_U8, Size, _U8, Size);
typedef _NoiseInitD = _V Function(_U8, int, _U8, int);
typedef _NoiseAcceptC = _V Function(_U8, Size, _U8, Size, _U8, Size);
typedef _NoiseAcceptD = _V Function(_U8, int, _U8, int, _U8, int);
typedef _AdmChalC = _BR Function(Uint32);
typedef _AdmChalD = _BR Function(int);
typedef _AdmAnswerC = _BR Function(_U8, Size, _U8, Size, _U8, Size);
typedef _AdmAnswerD = _BR Function(_U8, int, _U8, int, _U8, int);
typedef _AdmGrantC = Int32 Function(_V, Uint32, _U8, Size, _U8, Size, Uint64);
typedef _AdmGrantD = int Function(_V, int, _U8, int, _U8, int, int);
