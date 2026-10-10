part of '../../aviary.dart';

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
