part of '../../aviary.dart';

typedef _NetNewC = Pointer<Void> Function(Pointer<Uint8>, Size, Uint32,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint64);
typedef _NetNewD = Pointer<Void> Function(
    Pointer<Uint8>, int, int, Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _NetPeerC = Void Function(Pointer<Void>, Uint32);
typedef _NetPeerD = void Function(Pointer<Void>, int);
typedef _NetPeerFlagC = Int32 Function(Pointer<Void>, Uint32);
typedef _NetPeerFlagD = int Function(Pointer<Void>, int);
typedef _NetGossipC = _BufResult Function(
    Pointer<Void>, Uint32, Pointer<Uint8>, Size);
typedef _NetGossipD = _BufResult Function(
    Pointer<Void>, int, Pointer<Uint8>, int);
typedef _NetTimeoutC = _BufResult Function(Pointer<Void>, Uint8, Uint32);
typedef _NetTimeoutD = _BufResult Function(Pointer<Void>, int, int);
typedef _HandleBytesIntC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _HandleBytesIntD = int Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _HandleFlagC = Int32 Function(Pointer<Void>);
typedef _HandleFlagD = int Function(Pointer<Void>);
typedef _BufNoArgC = _BufResult Function();
typedef _BufNoArgD = _BufResult Function();
typedef _SecInitC = Pointer<Void> Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _SecInitD = Pointer<Void> Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _SecAcceptC = Pointer<Void> Function(
    Pointer<Uint8>,
    Size,
    Pointer<Uint8>,
    Size,
    Pointer<Uint8>,
    Size,
    Pointer<Uint8>,
    Size,
    Pointer<Uint8>,
    Size);
typedef _SecAcceptD = Pointer<Void> Function(
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int,
    Pointer<Uint8>,
    int);
