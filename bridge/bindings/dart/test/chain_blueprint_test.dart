import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void check(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

Uint8List fill(int b, [int n = 32]) => Uint8List(n)..fillRange(0, n, b);
bool eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

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
    final accounts = [(address: fill(0x11), balance: 1000)];
    final validators = [(publicKey: v.public, power: 1)];
    final genesis = bc.buildGenesis(
      chainId: 7,
      accounts: accounts,
      validators: validators,
    );

    final pos = bc.makeBlueprint(
      consensus: 'pos',
      stateModel: 'account',
      proposer: 'round-robin',
    );
    check(pos.hash().length == 32, 'blueprint hash');
    final mixed = bc.commitBlueprint(genesis, pos);
    check(
      !eq(bc.genesisHash(genesis), bc.genesisHash(mixed)),
      'the mixture changes the genesis hash',
    );

    final chain = bc.assembleChain(mixed, 0, v.public, v.secret);
    check(chain != null, 'PoS chain assembles');
    check(
      eq(chain!.blueprint(), pos.encoded),
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

    check(
      bc.assembleChain(
            bc.commitBlueprint(
              genesis,
              bc.makeBlueprint(
                consensus: 'no-such-engine',
                stateModel: 'account',
              ),
            ),
            0,
            v.public,
            v.secret,
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
    check(
      bc.assembleChain(
            bc.commitBlueprint(hardened, pos),
            0,
            v.public,
            v.secret,
          ) ==
          null,
      'hardened posture rejects non-grind-proof engine',
    );
    check(
      bc.assembleChain(
            bc.commitBlueprint(
              genesis,
              bc.makeBlueprint(consensus: 'pos', stateModel: 'utxo'),
            ),
            0,
            v.public,
            v.secret,
          ) ==
          null,
      'state mismatch rejected',
    );
    print(
      'aviary dart chain-blueprint smoke: OK (discovery, mixture=identity, PoS+PoW commit, fail-closed rejects)',
    );
  });
}
