part of '../../aviary.dart';

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
