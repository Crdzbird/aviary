import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:blake3_ffi/blake3_ffi.dart' as b3;
import 'package:crypto/crypto.dart' as cr;
import 'package:hashlib/hashlib.dart' as hl;

import 'measure.dart';

/// Hash rates for the inputs a ledger hashes: a 32 byte node pair half, a
/// 1 KiB transaction and a 64 KiB block body.
Map<String, Future<double> Function()> hashing(Aviary bc) {
  final contenders = <String, List<int> Function(Uint8List)>{
    'aviary_dart, BLAKE3 (FFI)': bc.hash,
    'blake3_ffi, BLAKE3 (FFI)': (d) => b3.blake3(d),
    'hashlib, BLAKE2b-256': (d) => hl.blake2b256.convert(d).bytes,
    'hashlib, SHA-256': (d) => hl.sha256.convert(d).bytes,
    'crypto, SHA-256': (d) => cr.sha256.convert(d).bytes,
  };
  final runs = <String, Future<double> Function()>{};
  for (final size in const [32, 1024, 65536]) {
    final input = bytesOf(size, size);
    final label = size < 1024 ? '$size B' : '${size ~/ 1024} KiB';
    contenders.forEach((name, f) {
      runs['Hash $label|$name'] = () async => rate((_) => consume(f(input)));
    });
  }
  return runs;
}
