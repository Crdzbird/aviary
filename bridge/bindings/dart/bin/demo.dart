import 'dart:io';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

void main(List<String> args) {
  final bc = args.isEmpty ? Aviary.instance : Aviary.open(args[0]);
  stdout.writeln('aviary ${bc.version()} (${bc.cryptoBackend()})');
  final digest = bc.hash(Uint8List.fromList('hello'.codeUnits));
  stdout.writeln(
      'blake3(hello) = ${digest.map((b) => b.toRadixString(16).padLeft(2, '0')).join()}');
  final node = bc.createNode(7);
  final alice = node.newAccount();
  final bob = node.newAccount();
  node.credit(alice.address, 1000);
  node.submitTransfer(
      alice.secret, bob.address, 250, node.nonce(alice.address));
  final height = node.produceBlock(1);
  stdout.writeln(
      'height $height, alice ${node.balance(alice.address)}, bob ${node.balance(bob.address)}');
  final proof = node.proveAccount(bob.address);
  stdout.writeln(
      'bob proof verifies: ${bc.verifyStateProof(node.stateRoot(), proof)}');
  node.dispose();
}
