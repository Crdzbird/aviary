part of '../../blockchainer.dart';

List<ValidatorEntry> _decodeValidatorSet(Uint8List b) {
  final bd = ByteData.sublistView(b);
  final n = bd.getUint32(0);
  final out = <ValidatorEntry>[];
  var off = 4;
  for (var i = 0; i < n; i++) {
    final l = bd.getUint32(off);
    off += 4;
    final pk = Uint8List.fromList(b.sublist(off, off + l));
    off += l;
    out.add((publicKey: pk, power: bd.getUint64(off)));
    off += 8;
  }
  return out;
}

Uint8List _encodeValidatorSet(List<ValidatorEntry> vs) {
  final b = BytesBuilder()..add(_u32Bytes(vs.length));
  for (final v in vs) {
    b
      ..add(_u32Bytes(v.publicKey.length))
      ..add(v.publicKey)
      ..add((ByteData(8)..setUint64(0, v.power)).buffer.asUint8List());
  }
  return b.toBytes();
}

Uint8List _publicKeys(List<ValidatorEntry> vs) =>
    _concat(vs.map((v) => v.publicKey));

List<int> _powers(List<ValidatorEntry> vs) => [for (final v in vs) v.power];
