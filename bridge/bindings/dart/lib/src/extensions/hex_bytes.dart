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
