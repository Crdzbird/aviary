part of '../../blockchainer.dart';

int _u32be(Uint8List b, int off) =>
    (b[off] << 24) | (b[off + 1] << 16) | (b[off + 2] << 8) | b[off + 3];

Uint8List _u32Bytes(int v) =>
    (ByteData(4)..setUint32(0, v)).buffer.asUint8List();

/// Encodes a list of byte strings as count, then length prefixed items.
Uint8List _blobList(List<Uint8List> items) {
  final b = BytesBuilder()..add(_u32Bytes(items.length));
  for (final it in items) {
    b
      ..add(_u32Bytes(it.length))
      ..add(it);
  }
  return b.toBytes();
}

List<Uint8List> _splitBlobList(Uint8List blob) {
  final n = _u32be(blob, 0);
  final out = <Uint8List>[];
  var off = 4;
  for (var i = 0; i < n; i++) {
    final len = _u32be(blob, off);
    off += 4;
    out.add(Uint8List.sublistView(blob, off, off + len));
    off += len;
  }
  return out;
}

({Uint8List public, Uint8List secret}) _splitKeyBlob(Uint8List blob) {
  final pl = _u32be(blob, 0);
  return (
    public: Uint8List.sublistView(blob, 4, 4 + pl),
    secret: Uint8List.sublistView(blob, 4 + pl),
  );
}

Uint8List _concat(Iterable<Uint8List> parts) {
  final b = BytesBuilder();
  parts.forEach(b.add);
  return b.toBytes();
}

/// The commitment part of a membership commit blob, for the issuer.
Uint8List extractCommitment(Uint8List membershipCommit) =>
    Uint8List.sublistView(membershipCommit, 4, 4 + _u32be(membershipCommit, 0));
