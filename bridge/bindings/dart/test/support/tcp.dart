import 'dart:io';
import 'dart:typed_data';

Uint8List frame(Uint8List payload) {
  final out = BytesBuilder()
    ..add((ByteData(4)..setUint32(0, payload.length)).buffer.asUint8List())
    ..add(payload);
  return out.toBytes();
}

Stream<Uint8List> frames(Socket socket) async* {
  var pending = Uint8List(0);
  await for (final chunk in socket) {
    final data = Uint8List.fromList([...pending, ...chunk]);
    var off = 0;
    while (data.length - off >= 4) {
      final len = ByteData.sublistView(data, off, off + 4).getUint32(0);
      if (data.length - off - 4 < len) break;
      yield Uint8List.sublistView(data, off + 4, off + 4 + len);
      off += 4 + len;
    }
    pending = Uint8List.sublistView(data, off);
  }
}

Future<void> waitUntil(bool Function() done, String what) async {
  for (var i = 0; i < 1000; i++) {
    if (done()) return;
    await Future<void>.delayed(const Duration(milliseconds: 10));
  }
  throw StateError('FAIL: timed out waiting for $what');
}
