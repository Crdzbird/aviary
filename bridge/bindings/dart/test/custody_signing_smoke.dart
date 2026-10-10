import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool fails(void Function() f) {
  try {
    f();
    return false;
  } on AviaryException catch (_) {
    return true;
  }
}

void main() {
  final bc = Aviary.instance;
  final node = bc.createNode(0x5151);
  final store = bc.createKeyStore('hunter2');
  final alice = node.newAccount();
  final hybrid = node.newAccountHybrid();
  final bob = node.fundAccount(0);
  final a = store.addAccount(alice.publicKey, alice.secret);
  final h = store.addAccount(hybrid.publicKey, hybrid.secret);
  expect(a.equalsBytes(alice.address) && h.equalsBytes(hybrid.address),
      'store addresses match the accounts');
  node.credit(a, 1000);
  node.credit(h, 500);

  expect(store.has(a) && !store.has(bob.address), 'inventory');
  expect(store.addresses().length == 2, 'two stored addresses');
  expect(store.publicKey(h).equalsBytes(hybrid.publicKey), 'public key kept');

  node.sendFromStore(store, a, bob.address, 100);
  node.submitTransferFromStore(store, h, bob.address, 50, 0);
  final signed = store.signTransfer(
      chainId: 0x5151, from: a, to: bob.address, amount: 1, nonce: 1, fee: 3);
  final txid = node.submitTx(signed);
  expect(txid.length == 32, 'encoded transaction admitted');
  expect(node.mempoolSize() == 3, 'three custody signed transfers pending');
  node.commit();
  expect(node.balance(bob.address) == 151, 'ed25519, hybrid and fee applied');
  expect(node.balance(a) == 1000 - 100 - 1 - 3, 'fee burned from alice');

  store.lock();
  expect(!store.unlocked, 'locked');
  expect(fails(() => node.sendFromStore(store, a, bob.address, 1)),
      'locked store cannot sign');
  store.unlock('hunter2');
  expect(fails(() => node.sendFromStore(store, bob.address, a, 1)),
      'unknown account cannot sign');
  expect(fails(() => store.publicKey(bob.address)), 'unknown public key');
  expect(fails(() => node.submitTx(Uint8List.fromList([1, 2, 3]))),
      'garbage transaction refused');

  final reloaded = bc.loadKeyStore(store.serialise());
  expect(!reloaded.unlocked, 'reloaded store is locked');
  reloaded.unlock('hunter2');
  node.sendFromStore(reloaded, a, bob.address, 2);
  node.commit();
  expect(node.balance(bob.address) == 153, 'reloaded store signs');

  final validator = bc.newValidator();
  final genesis = bc.buildGenesis(
    chainId: 9,
    accounts: [(address: a, balance: 10)],
    validators: [validator.entry(1)],
  );
  final net = bc.newNetNode(genesis: genesis, me: 0, blsKey: validator);
  net.setTransactions([
    store.signTransfer(
        chainId: 9, from: a, to: bob.address, amount: 4, nonce: 0),
  ]);
  net.start();
  expect(net.committed && net.balance(bob.address) == 4,
      'custody signed transfer carried by a networked node');
  net.free();
  for (final s in [store, reloaded]) {
    s.dispose();
  }
  node.dispose();
  print('aviary dart custody signing smoke: OK');
}
