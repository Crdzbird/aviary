import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('custody signing', () {
    final bc = Aviary.instance;
    final node = bc.createNode(0x5151);
    final store = bc.createKeyStore('hunter2');
    final alice = node.newAccount();
    final hybrid = node.newAccountHybrid();
    final bob = node.fundAccount(0);
    final a = store.addAccount(alice.publicKey, alice.secret);
    final h = store.addAccount(hybrid.publicKey, hybrid.secret);
    check(
      a.equalsBytes(alice.address) && h.equalsBytes(hybrid.address),
      'store addresses match the accounts',
    );
    node.credit(a, 1000);
    node.credit(h, 500);

    check(store.has(a) && !store.has(bob.address), 'inventory');
    check(store.addresses().length == 2, 'two stored addresses');
    check(store.publicKey(h).equalsBytes(hybrid.publicKey), 'public key kept');

    node.sendFromStore(store, a, bob.address, 100);
    node.submitTransferFromStore(store, h, bob.address, 50, 0);
    final signed = store.signTransfer(
      chainId: 0x5151,
      from: a,
      to: bob.address,
      amount: 1,
      nonce: 1,
      fee: 3,
    );
    final txid = node.submitTx(signed);
    check(txid.length == 32, 'encoded transaction admitted');
    check(node.mempoolSize() == 3, 'three custody signed transfers pending');
    node.commit();
    check(node.balance(bob.address) == 151, 'ed25519, hybrid and fee applied');
    check(node.balance(a) == 1000 - 100 - 1 - 3, 'fee burned from alice');

    store.lock();
    check(!store.unlocked, 'locked');
    check(
      fails(() => node.sendFromStore(store, a, bob.address, 1)),
      'locked store cannot sign',
    );
    store.unlock('hunter2');
    check(
      fails(() => node.sendFromStore(store, bob.address, a, 1)),
      'unknown account cannot sign',
    );
    check(fails(() => store.publicKey(bob.address)), 'unknown public key');
    check(
      fails(() => node.submitTx(Uint8List.fromList([1, 2, 3]))),
      'garbage transaction refused',
    );

    for (final s in [store]) {
      s.dispose();
    }
    node.dispose();
    print('aviary dart custody signing smoke: OK');
  });
}
