import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';

/// A root with two children and a grandchild holding [alice]'s balance.
final class TreeFixture {
  TreeFixture(this.bc)
    : pos = bc.makeBlueprint(
        consensus: 'pos',
        stateModel: 'account',
        proposer: 'round-robin',
      );

  final Aviary bc;
  final Blueprint pos;
  final Uint8List alice = fill(0xA1);

  TreeValidator validator(int seed) {
    final v = bc.newBLSValidator(fill(seed));
    return (publicKey: v.public, secret: v.secret, power: 1);
  }

  TreeNode node() => TreeNode(
    chainId: 1,
    blueprint: pos,
    validators: [validator(0x01)],
    children: [
      TreeNode(
        chainId: 42,
        validators: [validator(0x10), validator(0x11), validator(0x12)],
        maxHeadersPerAnchor: 4,
        children: [
          TreeNode(
            chainId: 420,
            validators: [validator(0x20), validator(0x21)],
            accounts: [(address: alice, balance: 1000)],
          ),
        ],
      ),
      TreeNode(chainId: 43, validators: [validator(0x30)]),
    ],
  );
}
