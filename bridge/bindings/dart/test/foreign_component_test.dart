import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/dart_hash_engine.dart';

void main() {
  test('foreign component', () {
    final bc = Aviary.instance;
    final eng = DartHashEngine();
    check(bc.registerEngine('dart-engine', eng), 'register Dart engine');
    check(
      bc.registerProposer('dart-first', (h, n) => 0),
      'register Dart proposer',
    );
    check(
      bc.engineIds().contains('dart-engine') &&
          bc.proposerIds().contains('dart-first'),
      'discoverable',
    );
    final v = bc.newBLSValidator(fill(0x60));
    final genesis = bc.buildGenesis(
      chainId: 7,
      accounts: [(address: fill(0x11), balance: 1000)],
      validators: [(publicKey: v.public, power: 1)],
    );
    Chain assemble(Blueprint bp) => bc.assembleChain(
      bc.commitBlueprint(genesis, bp),
      0,
      v.public,
      v.secret,
    )!;
    final chain = assemble(
      bc.makeBlueprint(consensus: 'dart-engine', stateModel: 'account'),
    );
    final blk = chain.propose(2);
    final just = chain.seal(blk);
    check(
      String.fromCharCodes(just.sublist(0, 10)) == 'dart-just:',
      'seal via Dart justify',
    );
    check(
      !chain.submit(blk, tampered(just)) && chain.height == 0,
      'tampered justification refused, ledger untouched',
    );
    check(
      chain.submit(blk, just) && chain.height == 1,
      'Dart-justified block committed',
    );
    check(
      eng.justified >= 1 && eng.verified >= 2,
      'callbacks crossed the boundary',
    );
    chain.free();
    final pc = assemble(
      bc.makeBlueprint(
        consensus: 'pos',
        stateModel: 'account',
        proposer: 'dart-first',
      ),
    );
    final pb = pc.propose(2);
    check(
      pc.submit(pb, pc.seal(pb)) && pc.height == 1,
      'PoS with a Dart proposer committed',
    );
    pc.free();
    print('aviary dart foreign-component smoke: OK');
  });
}
