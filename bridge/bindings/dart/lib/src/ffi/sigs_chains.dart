part of '../../aviary.dart';

typedef _BpEncodeC =
    _BR Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, _U8, Size);
typedef _BpEncodeD =
    _BR Function(Pointer<Utf8>, Pointer<Utf8>, Pointer<Utf8>, _U8, int);
typedef _RegistryIdsC = _BR Function(Uint8);
typedef _RegistryIdsD = _BR Function(int);
typedef _GenesisBuildC =
    _BR Function(
      Uint64,
      Uint64,
      Uint16,
      Uint8,
      _U8,
      Pointer<Uint64>,
      Size,
      _U8,
      Pointer<Uint64>,
      Size,
    );
typedef _GenesisBuildD =
    _BR Function(
      int,
      int,
      int,
      int,
      _U8,
      Pointer<Uint64>,
      int,
      _U8,
      Pointer<Uint64>,
      int,
    );
typedef _ChainAssembleC = _V Function(_U8, Size, Uint32, _U8, Size, _U8, Size);
typedef _ChainAssembleD = _V Function(_U8, int, int, _U8, int, _U8, int);
typedef _ChainProposeC = _BR Function(_V, Uint64);
typedef _ChainProposeD = _BR Function(_V, int);
typedef _ChainBlockC = _BR Function(_V, _U8, Size);
typedef _ChainBlockD = _BR Function(_V, _U8, int);
typedef _ChainSubmitC = Int32 Function(_V, _U8, Size, _U8, Size);
typedef _ChainSubmitD = int Function(_V, _U8, int, _U8, int);
typedef _ScSchedC = Int32 Function(_V, _U8, Pointer<Uint64>, Size);
typedef _ScSchedD = int Function(_V, _U8, Pointer<Uint64>, int);

typedef _ScChildRegC =
    _BR Function(Uint64, _U8, Size, _U8, Pointer<Uint64>, Size, Uint8);
typedef _ScChildRegD =
    _BR Function(int, _U8, int, _U8, Pointer<Uint64>, int, int);
typedef _ScPolicyC = _BR Function(Uint16, Uint8, Uint16, Uint16, _U8, Size);
typedef _ScPolicyD = _BR Function(int, int, int, int, _U8, int);
typedef _ScBindingC = _BR Function(_U8, Size, Uint64, _U8);
typedef _ScBindingD = _BR Function(_U8, int, int, _U8);
typedef _ScAnchorC = _BR Function(Uint64, _U8, Size);
typedef _ScAnchorD = _BR Function(int, _U8, int);
typedef _ScChainAnchoredC = _BR Function(_V, Uint64, _U8, Size);
typedef _ScChainAnchoredD = _BR Function(_V, int, _U8, int);
typedef _ScChainIdC = _BR Function(_V, Uint64);
typedef _ScChainIdD = _BR Function(_V, int);
typedef _ScChainAddrC = _BR Function(_V, _U8);
typedef _ScChainAddrD = _BR Function(_V, _U8);

typedef _LcNewC = _V Function(Uint64, _U8, Size, _U8, Size);
typedef _LcNewD = _V Function(int, _U8, int, _U8, int);
typedef _LcAcceptC = Int32 Function(_V, _U8, Size, _U8, Size);
typedef _LcAcceptD = int Function(_V, _U8, int, _U8, int);
typedef _LcSkipC = Int32 Function(_V, _U8, Size, _U8, Size, _U8, Size);
typedef _LcSkipD = int Function(_V, _U8, int, _U8, int, _U8, int);
typedef _LcTrustC = Void Function(_V, Uint32, Uint32);
typedef _LcTrustD = void Function(_V, int, int);
typedef _LcMaxSkipC = Void Function(_V, Uint64);
typedef _LcMaxSkipD = void Function(_V, int);

typedef _JustifyC = Int32 Function(_V, _U8, Size, _U8, Size, Pointer<Size>);
typedef _FVerifyC = Int32 Function(_V, _U8, Size, _U8, Size);
typedef _ProposerC = Uint32 Function(_V, Uint64, Uint32);
typedef _RegEngineC =
    Int32 Function(
      Pointer<Utf8>,
      Pointer<NativeFunction<_JustifyC>>,
      Pointer<NativeFunction<_FVerifyC>>,
      _V,
      Int32,
    );
typedef _RegEngineD =
    int Function(
      Pointer<Utf8>,
      Pointer<NativeFunction<_JustifyC>>,
      Pointer<NativeFunction<_FVerifyC>>,
      _V,
      int,
    );
typedef _RegProposerC =
    Int32 Function(Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, _V);
typedef _RegProposerD =
    int Function(Pointer<Utf8>, Pointer<NativeFunction<_ProposerC>>, _V);
