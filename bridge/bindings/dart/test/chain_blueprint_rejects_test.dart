import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';

void main() {
  test('chain blueprint rejects', () {
    final bc = Aviary.instance;
    final v = bc.newBLSValidator(fill(0x60));
    final accounts = [(address: fill(0x11), balance: 1000)];
    final validators = [(publicKey: v.public, power: 1)];
    final genesis = bc.buildGenesis(
      chainId: 7,
      accounts: accounts,
      validators: validators,
    );
    Chain? assemble(Blueprint bp, [Uint8List? gen]) => bc.assembleChain(
      bc.commitBlueprint(gen ?? genesis, bp),
      0,
      v.public,
      v.secret,
    );
    check(
      assemble(
            bc.makeBlueprint(
              consensus: 'no-such-engine',
              stateModel: 'account',
            ),
          ) ==
          null,
      'unknown engine rejected',
    );
    final hardened = bc.buildGenesis(
      chainId: 7,
      assuranceLevel: 2,
      accounts: accounts,
      validators: validators,
    );
    final pos = bc.makeBlueprint(
      consensus: 'pos',
      stateModel: 'account',
      proposer: 'round-robin',
    );
    check(
      assemble(pos, hardened) == null,
      'hardened posture rejects non-grind-proof engine',
    );
    check(
      assemble(bc.makeBlueprint(consensus: 'pos', stateModel: 'utxo')) == null,
      'state mismatch rejected',
    );
    print('aviary dart chain-blueprint rejects smoke: OK');
  });
}
