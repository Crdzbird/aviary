import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';

/// Two BFT nodes on chain 8: block 1 rotates from validator A alone to A
/// (power 1) plus B (power 3), then B produces two more blocks. The
/// checkpoints at heights 1 to 3 carry the set in force at each height.
final class RotationFixture {
  RotationFixture(this.bc)
    : va = bc.newBLSValidator(fill(0x91)),
      vb = bc.newBLSValidator(fill(0x92)) {
    genesisSet = [(publicKey: va.public, power: 1)];
    genesis = bc.commitBlueprint(
      bc.buildGenesis(chainId: 8, accounts: [], validators: genesisSet),
      bc.makeBlueprint(consensus: 'bft', stateModel: 'account'),
    );
    a = bc.assembleChain(genesis, 0, va.public, va.secret)!;
    b = bc.assembleChain(genesis, 0, vb.public, vb.secret)!;
    a.scheduleValidators([
      (publicKey: va.public, power: 1),
      (publicKey: vb.public, power: 3),
    ]);
    final b1 = a.propose(2);
    final c1 = a.certify(b1);
    if (!a.submit(b1, c1) || !b.submit(b1, c1)) {
      throw StateError('rotation block refused');
    }
    checkpoints[1] = (
      header: bc.blockHeader(b1),
      certificate: c1,
      validators: genesisSet,
    );
    newSet = b.validators();
    for (var ts = 3; ts <= 4; ts++) {
      final blk = b.propose(ts);
      final cert = b.certify(blk);
      if (!b.submit(blk, cert) || !a.submit(blk, cert)) {
        throw StateError('B block refused');
      }
      checkpoints[ts - 1] = (
        header: bc.blockHeader(blk),
        certificate: cert,
        validators: newSet,
      );
    }
  }

  final Aviary bc;
  final KeyPairBytes va;
  final KeyPairBytes vb;
  late final List<ValidatorEntry> genesisSet;
  late final List<ValidatorEntry> newSet;
  late final Uint8List genesis;
  late final Chain a;
  late final Chain b;
  final Map<int, Checkpoint> checkpoints = {};

  Uint8List get genesisHeader => bc.genesisHeader(genesis);

  LightClient lightClient() => bc.newLightClient(8, genesisSet, genesisHeader)!;

  void free() {
    a.free();
    b.free();
  }
}
