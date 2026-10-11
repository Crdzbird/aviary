import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/subchain_fixture.dart';

void main() {
  test('subchain proof', () {
    final bc = Aviary.instance;
    final fx = SubchainFixture(bc);
    fx.produceChild(2);
    fx.produceChild(3);
    final pblk = fx.anchor(2, fx.certified);
    final root = bc.headerSubchainRoot(bc.blockHeader(pblk));
    final proof = fx.proof();
    check(bc.verifySubchainProof(root, proof), 'cross-chain proof verifies');
    check(
      !bc.verifySubchainProof(fill(1), proof) &&
          !bc.verifySubchainProof(root, tampered(proof)) &&
          !bc.verifySubchainProof(
            root,
            bc.encodeSubchainProof([], fx.child.proveAccount(fx.alice)),
          ),
      'tampered / rootless proofs fail',
    );
    final header3 = fx.produceChild(4);
    final pblk2 = fx.anchor(3, [header3]);
    final root2 = bc.headerSubchainRoot(bc.blockHeader(pblk2));
    check(
      !bc.verifySubchainProof(root2, proof) &&
          bc.verifySubchainProof(root, proof),
      'old proof stale at new root, valid at old',
    );
    check(bc.verifySubchainProof(root2, fx.proof()), 'fresh proof at new root');
    fx.free();
    print('aviary dart subchain proof smoke: OK');
  });
}
