part of '../../aviary.dart';

typedef _BpEncodeC = _BufResult Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _BpEncodeD = _BufResult Function(
    Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, Pointer<Uint8>, int);
typedef _RegistryIdsC = _BufResult Function(Uint8);
typedef _RegistryIdsD = _BufResult Function(int);
typedef _GenesisBuildC = _BufResult Function(
    Uint64,
    Uint64,
    Uint16,
    Uint8,
    Pointer<Uint8>,
    Pointer<Uint64>,
    Size,
    Pointer<Uint8>,
    Pointer<Uint64>,
    Size);
typedef _GenesisBuildD = _BufResult Function(int, int, int, int, Pointer<Uint8>,
    Pointer<Uint64>, int, Pointer<Uint8>, Pointer<Uint64>, int);
typedef _ChainAssembleC = Pointer<Void> Function(
    Pointer<Uint8>, Size, Uint32, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ChainAssembleD = Pointer<Void> Function(
    Pointer<Uint8>, int, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ChainProposeC = _BufResult Function(Pointer<Void>, Uint64);
typedef _ChainProposeD = _BufResult Function(Pointer<Void>, int);
typedef _ChainBlockC = _BufResult Function(Pointer<Void>, Pointer<Uint8>, Size);
typedef _ChainBlockD = _BufResult Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _ChainSubmitC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ChainSubmitD = int Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _ScSchedC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Pointer<Uint64>, Size);
typedef _ScSchedD = int Function(
    Pointer<Void>, Pointer<Uint8>, Pointer<Uint64>, int);
