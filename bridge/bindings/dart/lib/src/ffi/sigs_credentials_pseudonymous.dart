part of '../../blockchainer.dart';

typedef _CredPolicyEncodeExC = _BufResult Function(
    Uint64, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint32, Uint8, Uint8);
typedef _CredPolicyEncodeExD = _BufResult Function(
    int, Pointer<Uint8>, int, Pointer<Uint8>, int, int, int, int);
typedef _BlindIssueC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Uint8>,
    Size, Pointer<Uint8>, Size, Uint8, Uint64, Uint32);
typedef _BlindIssueD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int, int, int, int);
typedef _CredGateAdmitPseudC = _BufResult Function(Pointer<Void>, Uint32,
    Pointer<Utf8>, Pointer<Uint8>, Size, Pointer<Uint8>, Size, Uint64);
typedef _CredGateAdmitPseudD = _BufResult Function(Pointer<Void>, int,
    Pointer<Utf8>, Pointer<Uint8>, int, Pointer<Uint8>, int, int);
typedef _CredGateRevokeNymC = Void Function(
    Pointer<Void>, Pointer<Uint8>, Size);
typedef _CredGateRevokeNymD = void Function(Pointer<Void>, Pointer<Uint8>, int);
typedef _CredGateNoteAdmC = Int32 Function(
    Pointer<Void>, Uint32, Pointer<Utf8>, Pointer<Uint8>, Size);
typedef _CredGateNoteAdmD = int Function(
    Pointer<Void>, int, Pointer<Utf8>, Pointer<Uint8>, int);
