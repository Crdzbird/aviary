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

typedef _ScChildRegC = _BufResult Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Pointer<Uint64>, Size, Uint8);
typedef _ScChildRegD = _BufResult Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, Pointer<Uint64>, int, int);
typedef _ScPolicyC = _BufResult Function(
    Uint16, Uint8, Uint16, Uint16, Pointer<Uint8>, Size);
typedef _ScPolicyD = _BufResult Function(
    int, int, int, int, Pointer<Uint8>, int);
typedef _ScBindingC = _BufResult Function(
    Pointer<Uint8>, Size, Uint64, Pointer<Uint8>);
typedef _ScBindingD = _BufResult Function(
    Pointer<Uint8>, int, int, Pointer<Uint8>);
typedef _ScAnchorC = _BufResult Function(Uint64, Pointer<Uint8>, Size);
typedef _ScAnchorD = _BufResult Function(int, Pointer<Uint8>, int);
typedef _ScChainAnchoredC = _BufResult Function(
    Pointer<Void>, Uint64, Pointer<Uint8>, Size);
typedef _ScChainAnchoredD = _BufResult Function(
    Pointer<Void>, int, Pointer<Uint8>, int);
typedef _ScChainIdC = _BufResult Function(Pointer<Void>, Uint64);
typedef _ScChainIdD = _BufResult Function(Pointer<Void>, int);
typedef _ScChainAddrC = _BufResult Function(Pointer<Void>, Pointer<Uint8>);
typedef _ScChainAddrD = _BufResult Function(Pointer<Void>, Pointer<Uint8>);

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

typedef _JustifyC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Size>);
typedef _FVerifyC = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _ProposerC = Uint32 Function(Pointer<Void>, Uint64, Uint32);
typedef _RegEngineC = Int32 Function(
    Pointer<Utf8>,
    Pointer<NativeFunction<_JustifyC>>,
    Pointer<NativeFunction<_FVerifyC>>,
    Pointer<Void>,
    Int32);
typedef _RegEngineD = int Function(
    Pointer<Utf8>,
    Pointer<NativeFunction<_JustifyC>>,
    Pointer<NativeFunction<_FVerifyC>>,
    Pointer<Void>,
    int);
typedef _RegProposerC = Int32 Function(
    Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, Pointer<Void>);
typedef _RegProposerD = int Function(
    Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, Pointer<Void>);
