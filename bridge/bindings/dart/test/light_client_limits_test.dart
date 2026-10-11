import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';
import 'support/rotation_fixture.dart';

void main() {
  test('light client limits', () {
    final bc = Aviary.instance;
    final fx = RotationFixture(bc);
    final cps = fx.checkpoints;
    final lc = fx.lightClient();
    lc.setMaxSkip(1);
    check(
      lc.acceptSkipping(cps[2]!) == SkipResult.refused,
      'max_skip refuses a 2-block jump',
    );
    lc.setMaxSkip(0);
    check(
      failsAnyhow(() => lc.syncTo(cps[3]!, 3, (_) => null)),
      'unavailable checkpoint fails',
    );
    check(
      bc.newLightClient(8, [], fx.genesisHeader) == null,
      'empty set refused',
    );
    lc.free();
    fx.free();
    print('aviary dart light-client limits smoke: OK');
  });
}
