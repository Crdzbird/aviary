import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';

void main() {
  test('validator rotation', () {
    final bc = Aviary.instance;
    final me = bc.newBLSValidator(fill(0x81)),
        other = bc.newBLSValidator(fill(0x82));
    final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');
    final chain = bc.assembleChain(
      bc.commitBlueprint(
        bc.buildGenesis(
          chainId: 5,
          accounts: [],
          validators: [(publicKey: me.public, power: 1)],
        ),
        bft,
      ),
      0,
      me.public,
      me.secret,
    )!;
    check(
      chain.validators().length == 1 &&
          chain.validators()[0].publicKey.equalsBytes(me.public),
      'initial set',
    );
    final grown = [
      (publicKey: me.public, power: 3),
      (publicKey: other.public, power: 1),
    ];
    chain.scheduleValidators(grown);
    final b1 = chain.propose(2);
    check(chain.submit(b1, chain.certify(b1)), 'rotation block');
    final vs = chain.validators();
    check(
      vs.length == 2 &&
          vs[0].power == 3 &&
          vs[1].publicKey.equalsBytes(other.public),
      'set after rotation',
    );
    final b2 = chain.propose(3);
    check(
      chain.submit(b2, chain.certify(b2)) && chain.height == 2,
      'block under the new set',
    );
    check(
      failsAnyhow(() => chain.scheduleValidators([])) &&
          failsAnyhow(
            () => chain.scheduleValidators([(publicKey: me.public, power: 0)]),
          ),
      'malformed sets refused',
    );
    chain.scheduleValidators([(publicKey: other.public, power: 1)]);
    final b3 = chain.propose(4);
    check(
      chain.submit(b3, chain.certify(b3)) && chain.height == 3,
      'leaving block',
    );
    check(
      failsAnyhow(() => chain.certify(chain.propose(5))),
      'a node that left cannot certify',
    );
    final pc = bc.assembleChain(
      bc.commitBlueprint(
        bc.buildGenesis(
          chainId: 6,
          accounts: [],
          validators: [(publicKey: me.public, power: 1)],
        ),
        bc.makeBlueprint(
          consensus: 'pos',
          stateModel: 'account',
          proposer: 'round-robin',
        ),
      ),
      0,
      me.public,
      me.secret,
    )!;
    check(failsAnyhow(() => pc.scheduleValidators(grown)), 'PoS refuses');
    chain.free();
    pc.free();
    print('aviary dart validator-rotation smoke: OK');
  });
}
