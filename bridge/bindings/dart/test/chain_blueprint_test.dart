import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';

void main() {
  test('chain blueprint', () {
    final bc = Aviary.instance;
    final eng = bc.engineIds();
    check(
      eng.contains('bft') &&
          eng.contains('pow') &&
          eng.contains('pos') &&
          eng.contains('vrf-pos'),
      'engines listed',
    );
    check(
      bc.stateModelIds().contains('account') &&
          bc.proposerIds().contains('round-robin'),
      'states + proposers listed',
    );
    final v = bc.newBLSValidator(fill(0x60));
    check(v.public.length == 48 && v.secret.length == 32, 'BLS key sizes');
    final genesis = bc.buildGenesis(
      chainId: 7,
      accounts: [(address: fill(0x11), balance: 1000)],
      validators: [(publicKey: v.public, power: 1)],
    );
    final pos = bc.makeBlueprint(
      consensus: 'pos',
      stateModel: 'account',
      proposer: 'round-robin',
    );
    check(pos.hash().length == 32, 'blueprint hash');
    final mixed = bc.commitBlueprint(genesis, pos);
    check(
      !bc.genesisHash(genesis).equalsBytes(bc.genesisHash(mixed)),
      'the mixture changes the genesis hash',
    );
    final chain = bc.assembleChain(mixed, 0, v.public, v.secret);
    check(chain != null, 'PoS chain assembles');
    check(
      chain!.blueprint().equalsBytes(pos.encoded),
      'chain reports the committed mixture',
    );
    final blk = chain.propose(2);
    final just = chain.seal(blk);
    check(
      just.isNotEmpty && chain.submit(blk, just) && chain.height == 1,
      'PoS block committed',
    );
    chain.free();
    final pow = bc.makeBlueprint(
      consensus: 'pow',
      stateModel: 'account',
      params: fill(0xff),
    );
    final pc = bc.assembleChain(
      bc.commitBlueprint(genesis, pow),
      0,
      v.public,
      v.secret,
    )!;
    final cand = pc.propose(2);
    check(pc.seal(cand).isEmpty, 'PoW does not seal');
    check(pc.submit(pc.mine(cand)) && pc.height == 1, 'PoW block committed');
    pc.free();
    print('aviary dart chain-blueprint smoke: OK');
  });
}
