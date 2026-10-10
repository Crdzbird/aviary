part of '../../aviary.dart';

typedef _CredPolicyEncodeC = _BufResult Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Uint8);
typedef _CredPolicyEncodeD = _BufResult Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int);
typedef _CredIssueC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>,
    Size, Pointer<Uint8>, Size, Uint8, Uint64, Uint32);
typedef _CredIssueD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int, int, int, int);
typedef _CredPresentC = _BufResult Function(Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _CredPresentD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>,
    int, int, Pointer<Utf8>, Pointer<Uint8>, int);
typedef _CredGateNewC = Pointer<Void> Function(Pointer<Uint8>, Size);
typedef _CredGateNewD = Pointer<Void> Function(Pointer<Uint8>, int);
typedef _CredGateRevokeC = Void Function(Pointer<Void>, Uint32);
typedef _CredGateRevokeD = void Function(Pointer<Void>, int);
typedef _CredGateAdmitC = Int32 Function(Pointer<Void>, Uint32, Pointer<Utf8>,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint64);
typedef _CredGateAdmitD = int Function(Pointer<Void>, int, Pointer<Utf8>,
    Pointer<Uint8>, int, Pointer<Uint8>, int, int);
