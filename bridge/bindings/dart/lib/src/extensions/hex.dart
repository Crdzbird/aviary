part of '../../aviary.dart';

/// Hex rendering and comparison for byte strings such as addresses, hashes
/// and transaction ids. Not constant time; do not compare secrets with it.
extension HexBytes on Uint8List {
  /// Lowercase hex, two characters per byte.
  String get hex {
    final sb = StringBuffer();
    for (final b in this) {
      sb.write(b.toRadixString(16).padLeft(2, '0'));
    }
    return sb.toString();
  }

  /// The first and last four bytes in hex joined by two dots, for logs.
  String get shortHex {
    if (length <= 8) return hex;
    final head = Uint8List.sublistView(this, 0, 4).hex;
    final tail = Uint8List.sublistView(this, length - 4).hex;
    return '$head..$tail';
  }

  /// Whether [other] holds exactly the same bytes.
  bool equalsBytes(Uint8List other) {
    if (other.length != length) return false;
    for (var i = 0; i < length; i++) {
      if (this[i] != other[i]) return false;
    }
    return true;
  }
}

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

/// Convenience views of an account.
extension AccountTools on Account {
  /// The chain address in lowercase hex.
  String get addressHex => address.hex;

  /// Whether this is a hybrid post quantum account. Its secret is longer
  /// than the 64 bytes of an Ed25519 secret.
  bool get isHybrid => secret.length != 64;
}
