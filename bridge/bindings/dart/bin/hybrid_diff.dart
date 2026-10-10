import 'dart:convert';
import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';

const chainId = 0x9001;
const credit = 1000000;
const transfer = 250000;

Uint8List _u(String s) {
  final o = Uint8List(s.length ~/ 2);
  for (var i = 0; i < o.length; i++) {
    o[i] = int.parse(s.substring(i * 2, i * 2 + 2), radix: 16);
  }
  return o;
}

String _hex(Uint8List b) =>
    b.map((x) => x.toRadixString(16).padLeft(2, '0')).join();

void main(List<String> args) {
  final j = jsonDecode(args.first) as Map<String, dynamic>;
  final aSec = _u(j['aliceSecret'] as String);
  final aPub = _u(j['alicePublic'] as String);
  final bAddr = _u(j['bobAddress'] as String);
  final goRoot = j['stateRoot'] as String;

  final bc = Aviary.instance;
  final node = bc.createNode(chainId);
  final aAddr = node.accountAddress(aPub);
  node.credit(aAddr, credit);
  node.submitTransferHybrid(aSec, aPub, bAddr, transfer, 0);
  node.produceBlock(1);
  final dartRoot = _hex(node.stateRoot());
  node.dispose();

  print('  go   stateRoot $goRoot');
  print('  dart stateRoot $dartRoot');
  if (goRoot != dartRoot) {
    throw StateError('MISMATCH - hybrid state diverges across bindings');
  }
  print(
    'MATCH - post-quantum ledger state is byte-identical across Go and Dart',
  );
}
