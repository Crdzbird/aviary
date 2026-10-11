import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';
import 'support/tree_fixture.dart';

void main() {
  test('chain tree rejects', () {
    final bc = Aviary.instance;
    final fx = TreeFixture(bc);
    final base = fx.node();
    check(
      failsAnyhow(
        () => bc.buildChainTree(
          base.copyWith(
            children: [
              base.children[0],
              base.children[1].copyWith(chainId: 42),
            ],
          ),
        ),
      ),
      'duplicate id refused',
    );
    check(
      failsAnyhow(
        () => bc.buildChainTree(
          base.copyWith(
            children: [
              base.children[0].copyWith(blueprint: fx.pos),
              base.children[1],
            ],
          ),
        ),
      ),
      'PoS child refused',
    );
    check(
      failsAnyhow(
        () => bc.buildChainTree(
          base.copyWith(
            blueprint: bc.makeBlueprint(
              consensus: 'no-such-engine',
              stateModel: 'account',
            ),
          ),
        ),
      ),
      'unknown engine refused',
    );
    print('aviary dart chain-tree rejects smoke: OK');
  });
}
