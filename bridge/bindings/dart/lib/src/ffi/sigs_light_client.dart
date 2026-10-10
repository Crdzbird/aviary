part of '../../blockchainer.dart';

typedef _LcNewC = Pointer<Void> Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _LcNewD = Pointer<Void> Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcAcceptC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _LcAcceptD = int Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcSkipC = Int32 Function(Pointer<Void>, Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _LcSkipD = int Function(Pointer<Void>, Pointer<Uint8>, int,
    Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _LcTrustC = Void Function(Pointer<Void>, Uint32, Uint32);
typedef _LcTrustD = void Function(Pointer<Void>, int, int);
typedef _LcMaxSkipC = Void Function(Pointer<Void>, Uint64);
typedef _LcMaxSkipD = void Function(Pointer<Void>, int);
