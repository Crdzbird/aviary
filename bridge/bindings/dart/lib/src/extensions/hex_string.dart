part of '../../blockchainer.dart';

final RegExp _hexPattern = RegExp(r'^[0-9a-fA-F]*$');

/// Decoding of hex and text into bytes.
extension HexString on String {
  /// The bytes of a hex string, with or without a 0x prefix. Throws a
  /// [FormatException] on an odd length or a character that is not hex.
  Uint8List get hexBytes {
    final body = startsWith('0x') || startsWith('0X') ? substring(2) : this;
    if (body.length.isOdd) throw FormatException('odd hex length', this);
    if (!_hexPattern.hasMatch(body)) throw FormatException('not hex', this);
    final out = Uint8List(body.length ~/ 2);
    for (var i = 0; i < out.length; i++) {
      out[i] = int.parse(body.substring(2 * i, 2 * i + 2), radix: 16);
    }
    return out;
  }

  /// The UTF-8 bytes of this string.
  Uint8List get utf8Bytes => Uint8List.fromList(utf8.encode(this));
}
