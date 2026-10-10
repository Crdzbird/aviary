part of '../../aviary.dart';

typedef _KemKeygenC = _BufResult Function();
typedef _KemKeygenD = _BufResult Function();
typedef _PSealPassC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Utf8>, Uint64, Size);
typedef _PSealPassD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Utf8>, int, int);
typedef _POpenPassC = _BufResult Function(Pointer<Uint8>, Size, Pointer<Utf8>);
typedef _POpenPassD = _BufResult Function(Pointer<Uint8>, int, Pointer<Utf8>);
typedef _PSealToC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealToD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _PSealSignedC = _BufResult Function(Pointer<Uint8>, Size,
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _PSealSignedD = _BufResult Function(Pointer<Uint8>, int, Pointer<Uint8>,
    int, Pointer<Uint8>, int, Pointer<Uint8>, int);
typedef _POpenSignedC = _BufResult Function(
    Pointer<Uint8>, Size, Pointer<Uint8>, Size, Pointer<Uint8>, Size);
typedef _POpenSignedD = _BufResult Function(
    Pointer<Uint8>, int, Pointer<Uint8>, int, Pointer<Uint8>, int);
