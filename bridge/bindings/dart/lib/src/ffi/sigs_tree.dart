part of '../../aviary.dart';

typedef _TreeNodeC =
    _BR Function(
      Uint64,
      _U8,
      Size,
      Uint16,
      _U8,
      Pointer<Uint64>,
      Size,
      _U8,
      _U8,
      Pointer<Uint64>,
      Size,
      Uint16,
      Uint16,
      _U8,
      Size,
    );
typedef _TreeNodeD =
    _BR Function(
      int,
      _U8,
      int,
      int,
      _U8,
      Pointer<Uint64>,
      int,
      _U8,
      _U8,
      Pointer<Uint64>,
      int,
      int,
      int,
      _U8,
      int,
    );
typedef _TreeBuildC = _V Function(_U8, Size);
typedef _TreeBuildD = _V Function(_U8, int);
typedef _TreeVerifyC = Int32 Function(_V, _U8, Size);
typedef _TreeVerifyD = int Function(_V, _U8, int);
typedef _TreeU32IdC = Uint32 Function(_V, Uint64);
typedef _TreeU32IdD = int Function(_V, int);
typedef _TreeU64IdC = Uint64 Function(_V, Uint64);
typedef _TreeU64IdD = int Function(_V, int);
typedef _TreeProduceC = _BR Function(_V, Uint64, Uint64);
typedef _TreeProduceD = _BR Function(_V, int, int);
typedef _TreeProveC = _BR Function(_V, Uint64, _U8);
typedef _TreeProveD = _BR Function(_V, int, _U8);
typedef _TreeFinC = Void Function(_V, Uint64, _U8, Size);
typedef _TreeTxC = Int32 Function(_V, Uint64, Uint64, _U8, Size, Pointer<Size>);
typedef _TreeSetFinC =
    Void Function(_V, Pointer<NativeFunction<_TreeFinC>>, _V);
typedef _TreeSetFinD =
    void Function(_V, Pointer<NativeFunction<_TreeFinC>>, _V);
typedef _TreeSetTxC = Void Function(_V, Pointer<NativeFunction<_TreeTxC>>, _V);
typedef _TreeSetTxD = void Function(_V, Pointer<NativeFunction<_TreeTxC>>, _V);

typedef _CredPolicyEncodeC =
    _BR Function(Uint64, _U8, Size, _U8, Size, Uint32, Uint8);
typedef _CredPolicyEncodeD = _BR Function(int, _U8, int, _U8, int, int, int);
typedef _CredIssueC =
    _BR Function(_U8, Size, _U8, Size, _U8, Size, Uint8, Uint64, Uint32);
typedef _CredIssueD = _BR Function(_U8, int, _U8, int, _U8, int, int, int, int);
typedef _CredPresentC =
    _BR Function(_U8, Size, _U8, Size, Uint32, Pointer<Utf8>, _U8, Size);
typedef _CredPresentD =
    _BR Function(_U8, int, _U8, int, int, Pointer<Utf8>, _U8, int);
typedef _CredGateNewC = _V Function(_U8, Size);
typedef _CredGateNewD = _V Function(_U8, int);
typedef _CredGateRevokeC = Void Function(_V, Uint32);
typedef _CredGateRevokeD = void Function(_V, int);
typedef _CredGateAdmitC =
    Int32 Function(_V, Uint32, Pointer<Utf8>, _U8, Size, _U8, Size, Uint64);
typedef _CredGateAdmitD =
    int Function(_V, int, Pointer<Utf8>, _U8, int, _U8, int, int);

typedef _CredPolicyEncodeExC =
    _BR Function(Uint64, _U8, Size, _U8, Size, Uint32, Uint8, Uint8);
typedef _CredPolicyEncodeExD =
    _BR Function(int, _U8, int, _U8, int, int, int, int);
typedef _BlindIssueC =
    _BR Function(_U8, Size, _U8, Size, _U8, Size, Uint8, Uint64, Uint32);
typedef _BlindIssueD =
    _BR Function(_U8, int, _U8, int, _U8, int, int, int, int);
typedef _CredGateAdmitPseudC =
    _BR Function(_V, Uint32, Pointer<Utf8>, _U8, Size, _U8, Size, Uint64);
typedef _CredGateAdmitPseudD =
    _BR Function(_V, int, Pointer<Utf8>, _U8, int, _U8, int, int);
typedef _CredGateRevokeNymC = Void Function(_V, _U8, Size);
typedef _CredGateRevokeNymD = void Function(_V, _U8, int);
typedef _CredGateNoteAdmC =
    Int32 Function(_V, Uint32, Pointer<Utf8>, _U8, Size);
typedef _CredGateNoteAdmD = int Function(_V, int, Pointer<Utf8>, _U8, int);
