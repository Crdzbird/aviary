import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/subchain_fixture.dart';

void main() {
  test('subchain', () {
    final bc = Aviary.instance;
    final fx = SubchainFixture(bc);
    check(
      !bc
          .genesisHash(fx.plainParent)
          .equalsBytes(bc.genesisHash(fx.parentGenesis)),
      'policy changes the parent genesis hash',
    );
    final ownChild = bc.encodeSubchainPolicy(
      maxChildren: 1,
      children: [fx.registration(1)],
    );
    check(
      failsAnyhow(() => bc.commitSubchainPolicy(fx.parentGenesis, ownChild)),
      'own-chain child refused',
    );
    check(
      !bc.genesisHash(fx.bound).equalsBytes(bc.genesisHash(fx.childGenesis)) &&
          bc.genesisHeader(fx.bound).equalsBytes(fx.childHeader),
      'binding changes the hash, not the header',
    );
    fx.produceChild(2);
    fx.produceChild(3);
    check(
      fx.child.height == 2 && fx.parent.childTip(42).isNotEmpty,
      'child state',
    );
    check(
      failsAnyhow(() => fx.parent.childTip(99)) &&
          failsAnyhow(() => fx.child.childTip(42)),
      'unknown child / non-parent refused',
    );
    fx.anchor(2, fx.certified);
    check(fx.parent.height == 1, 'parent anchored');
    check(
      failsAnyhow(() => fx.anchor(3, [tampered(fx.certified[1])])),
      'tampered anchor refused',
    );
    check(
      failsAnyhow(() => fx.anchor(3, fx.certified)),
      'stale anchor refused',
    );
    fx.free();
    print('aviary dart subchain smoke: OK');
  });
}
