import 'package:blockchainer_dart/blockchainer.dart';

/// Funds two accounts, moves value between them, seals a block and proves
/// the receiver's balance to a light client.
void main() {
  final bc = Blockchainer.instance;
  final node = bc.createNode(7);

  final alice = node.fundAccount(1000);
  final bob = node.fundAccount(0);
  node.sendTo(alice, bob, 250);
  node.commit();

  print('alice ${node.balance(alice.address)} at ${alice.addressHex}');
  print('bob   ${node.balance(bob.address)} at ${bob.address.shortHex}');

  final proof = node.proveAccount(bob.address);
  print('proof verifies: ${node.verifyAccountProof(proof)}');
  print('state root ${node.stateRoot().hex}');

  for (final event in node.events()) {
    print('event ${event['type']}');
  }
  node.dispose();
}
