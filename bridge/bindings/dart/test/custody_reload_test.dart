import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('custody reload', () {
    final bc = Aviary.instance;
    final node = bc.createNode(0x5152);
    final store = bc.createKeyStore('hunter2');
    final alice = node.newAccount();
    final bob = node.fundAccount(0);
    final a = store.addAccount(alice.publicKey, alice.secret);
    node.credit(a, 100);

    final reloaded = bc.loadKeyStore(store.serialise());
    check(!reloaded.unlocked, 'reloaded store is locked');
    reloaded.unlock('hunter2');
    node.sendFromStore(reloaded, a, bob.address, 2);
    node.commit();
    check(node.balance(bob.address) == 2, 'reloaded store signs');

    final validator = bc.newValidator();
    final genesis = bc.buildGenesis(
      chainId: 9,
      accounts: [(address: a, balance: 10)],
      validators: [validator.entry(1)],
    );
    final net = bc.newNetNode(genesis: genesis, me: 0, blsKey: validator);
    net.setTransactions([
      store.signTransfer(
        chainId: 9,
        from: a,
        to: bob.address,
        amount: 4,
        nonce: 0,
      ),
    ]);
    net.start();
    check(
      net.committed && net.balance(bob.address) == 4,
      'custody signed transfer carried by a networked node',
    );
    net.free();
    for (final s in [store, reloaded]) {
      s.dispose();
    }
    node.dispose();
    print('aviary dart custody reload smoke: OK');
  });
}
