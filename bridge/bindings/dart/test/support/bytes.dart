import 'dart:typed_data';

/// Thirty two bytes (or [n]) of [b].
Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);

/// A copy of [bytes] with its last bit flipped.
Uint8List tampered(Uint8List bytes) =>
    Uint8List.fromList(bytes)..[bytes.length - 1] ^= 1;
