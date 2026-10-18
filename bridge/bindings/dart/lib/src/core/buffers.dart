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

Object _decodeRecord(Uint8List b, {required bool withKey}) {
  var o = 0;
  Uint8List take(int n) {
    final out = b.sublist(o, o + n);
    o += n;
    return out;
  }

  final key = withKey ? take(32) : null;
  final owner = take(32);
  final version = ByteData.sublistView(b, o, o + 8).getUint64(0);
  o += 8;
  final len = ByteData.sublistView(b, o, o + 4).getUint32(0);
  o += 4;
  final value = take(len);
  if (withKey) return (key: key!, owner: owner, version: version, value: value);
  return (owner: owner, version: version, value: value);
}

PeerRecordInfo _peerRecordInfo(List<Uint8List> f) {
  int be(Uint8List b) => b.fold(0, (a, x) => a << 8 | x);
  return (
    id: be(f[0]),
    host: utf8.decode(f[1]),
    port: be(f[2]),
    kemPublic: f[3],
    sigPublic: f[4],
    prekey: f[5],
  );
}
