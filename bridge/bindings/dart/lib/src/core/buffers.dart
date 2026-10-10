part of '../../aviary.dart';

extension _AviaryBuffers on Aviary {
  String _cString(String symbol) =>
      lib.lookupFunction<_PtrVoid, _PtrVoid>(symbol)().toDartString();

  Uint8List _take(_BufResult r, String op) {
    if (r.buf.data == nullptr) throw _takeError(r, op);
    final out = Uint8List.fromList(r.buf.data.asTypedList(r.buf.len));
    final tmp = calloc<_Buffer>();
    tmp.ref.data = r.buf.data;
    tmp.ref.len = r.buf.len;
    lib.lookupFunction<_BufFreeC, _BufFreeD>('aviary_buffer_free')(tmp);
    calloc.free(tmp);
    return out;
  }

  AviaryException _takeError(_BufResult r, String op) {
    if (r.error == nullptr) return AviaryException('$op failed');
    final message = r.error.toDartString();
    lib.lookupFunction<_StrFreeC, _StrFreeD>('aviary_str_free')(r.error);
    return AviaryException(message);
  }

  String _takeStr(_BufResult r, String op) => utf8.decode(_take(r, op));
}
