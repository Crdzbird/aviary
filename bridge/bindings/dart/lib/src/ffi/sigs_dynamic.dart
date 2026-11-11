part of '../../aviary.dart';

typedef _DynRegistrarC = _BR Function(Uint8, _U8, Size);
typedef _DynRegistrarD = _BR Function(int, _U8, int);
typedef _DynPolicyC =
    _BR Function(
      Uint8,
      Uint32,
      Uint16,
      Uint16,
      Uint16,
      Uint8,
      _U8,
      Size,
      _U8,
      Size,
    );
typedef _DynPolicyD =
    _BR Function(
      int,
      int,
      int,
      int,
      int,
      int,
      _U8,
      int,
      _U8,
      int,
    );
typedef _OpRegisterC = _BR Function(Uint32, Uint64, _U8, Size);
typedef _OpRegisterD = _BR Function(int, int, _U8, int);
typedef _OpCloseC = _BR Function(Uint32, Uint64, Uint64, Uint64, _U8);
typedef _OpCloseD = _BR Function(int, int, int, int, _U8);
typedef _OpRegistrarsC = _BR Function(Uint32, Uint64, Uint8, _U8, Size);
typedef _OpRegistrarsD = _BR Function(int, int, int, _U8, int);
typedef _OpDigestC = _BR Function(_U8, Size, Uint64, _U8);
typedef _OpDigestD = _BR Function(_U8, int, int, _U8);
typedef _OpSignC =
    _BR Function(
      _U8,
      Size,
      Uint64,
      _U8,
      Uint16,
      Uint8,
      _U8,
      Size,
    );
typedef _OpSignD = _BR Function(_U8, int, int, _U8, int, int, _U8, int);
typedef _OpApprovalC = _BR Function(_U8, Size, Uint16, _U8, Size);
typedef _OpApprovalD = _BR Function(_U8, int, int, _U8, int);
typedef _ProposeSubchainC = _BR Function(_V, Uint64, _U8, Size, _U8, Size);
typedef _ProposeSubchainD = _BR Function(_V, int, _U8, int, _U8, int);
typedef _RegistryCountsC =
    Int32 Function(
      _V,
      Pointer<Uint32>,
      Pointer<Uint32>,
      Pointer<Uint32>,
    );
typedef _RegistryCountsD =
    int Function(
      _V,
      Pointer<Uint32>,
      Pointer<Uint32>,
      Pointer<Uint32>,
    );
typedef _ChildProofVerifyC = _BR Function(_U8, _U8, Size);
typedef _ChildProofVerifyD = _BR Function(_U8, _U8, int);
typedef _PendingAnchorC = _BR Function(_V, _V, Uint16);
typedef _PendingAnchorD = _BR Function(_V, _V, int);
typedef _ChainRestoreC =
    _V Function(
      _U8,
      Size,
      Uint32,
      _U8,
      Size,
      _U8,
      Size,
      _U8,
      Size,
    );
typedef _ChainRestoreD =
    _V Function(
      _U8,
      int,
      int,
      _U8,
      int,
      _U8,
      int,
      _U8,
      int,
    );
typedef _ArchiveRangeC = Int32 Function(_V, Pointer<Uint64>, Pointer<Uint64>);
typedef _ArchiveRangeD = int Function(_V, Pointer<Uint64>, Pointer<Uint64>);
typedef _TreeRestoreC = _V Function(_U8, Size, _U8, Size);
typedef _TreeRestoreD = _V Function(_U8, int, _U8, int);
