part of '../aviary_web.dart';

const _twoTo32 = 0x100000000;

/// The high and low 32 bit halves of a 64 bit argument, as the web entry
/// points take it. A JavaScript number is exact up to 2 to the 53, which
/// bounds the values a browser can pass.
List<int> _halves(int value, String name) {
  RangeError.checkNotNegative(value, name);
  return [value ~/ _twoTo32, value % _twoTo32];
}

/// The little endian 32 bit word at [at] in WebAssembly memory.
int _word(Uint8List heap, int at) =>
    heap[at] +
    heap[at + 1] * 0x100 +
    heap[at + 2] * 0x10000 +
    heap[at + 3] * 0x1000000;

/// Throws unless each of [values] is exactly 32 bytes.
void _require32(List<Uint8List> values, String op) {
  for (final v in values) {
    if (v.length != 32) {
      throw AviaryException(
        '$op: hashes must be 32 bytes',
        AviaryStatus.badInput,
      );
    }
  }
}
