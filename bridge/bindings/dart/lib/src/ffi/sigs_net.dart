part of '../../aviary.dart';

typedef _NetNewC = _V Function(_U8, Size, Uint32, _U8, Size, _U8, Size, Uint64);
typedef _NetNewD = _V Function(_U8, int, int, _U8, int, _U8, int, int);
typedef _NetPeerC = Void Function(_V, Uint32);
typedef _NetPeerD = void Function(_V, int);
typedef _NetPeerFlagC = Int32 Function(_V, Uint32);
typedef _NetPeerFlagD = int Function(_V, int);
typedef _NetGossipC = _BR Function(_V, Uint32, _U8, Size);
typedef _NetGossipD = _BR Function(_V, int, _U8, int);
typedef _NetTimeoutC = _BR Function(_V, Uint8, Uint32);
typedef _NetTimeoutD = _BR Function(_V, int, int);
typedef _HandleBytesIntC = Int32 Function(_V, _U8, Size);
typedef _HandleBytesIntD = int Function(_V, _U8, int);
typedef _HandleFlagC = Int32 Function(_V);
typedef _HandleFlagD = int Function(_V);
typedef _BufNoArgC = _BR Function();
typedef _BufNoArgD = _BR Function();
typedef _SecInitC = _V Function(_U8, Size, _U8, Size, _U8, Size);
typedef _SecInitD = _V Function(_U8, int, _U8, int, _U8, int);
typedef _SecAcceptC =
    _V Function(_U8, Size, _U8, Size, _U8, Size, _U8, Size, _U8, Size);
typedef _SecAcceptD =
    _V Function(_U8, int, _U8, int, _U8, int, _U8, int, _U8, int);
