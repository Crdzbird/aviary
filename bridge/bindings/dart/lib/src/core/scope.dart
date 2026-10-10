part of '../../aviary.dart';

Pointer<Uint8> _allocBytes(Uint8List d) {
  final p = calloc<Uint8>(d.isEmpty ? 1 : d.length);
  if (d.isNotEmpty) p.asTypedList(d.length).setAll(0, d);
  return p;
}

R _scoped<R>(List<Uint8List> inputs, R Function(List<Pointer<Uint8>>) body) {
  final ptrs = [for (final d in inputs) _allocBytes(d)];
  try {
    return body(ptrs);
  } finally {
    ptrs.forEach(calloc.free);
  }
}

R _withUtf8<R>(String s, R Function(Pointer<Utf8>) body) {
  final p = s.toNativeUtf8();
  try {
    return body(p);
  } finally {
    calloc.free(p);
  }
}

R _withU64s<R>(List<int> values, R Function(Pointer<Uint64>) body) {
  final p = calloc<Uint64>(values.isEmpty ? 1 : values.length);
  for (var i = 0; i < values.length; i++) {
    p[i] = values[i];
  }
  try {
    return body(p);
  } finally {
    calloc.free(p);
  }
}

Pointer<Void> _requireHandle(Pointer<Void> h, String what) {
  if (h == nullptr) throw AviaryException('$what failed');
  return h;
}
