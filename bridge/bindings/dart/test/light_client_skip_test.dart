import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/bytes.dart';
import 'support/check.dart';
import 'support/rotation_fixture.dart';

void main() {
  test('light client skip', () {
    final bc = Aviary.instance;
    final fx = RotationFixture(bc);
    final cps = fx.checkpoints;
    final lc = fx.lightClient();
    check(
      lc.acceptSkipping(cps[3]!) == SkipResult.insufficientOverlap,
      'no-overlap skip reports thin overlap',
    );
    check(
      lc.acceptSkipping((
            header: cps[3]!.header,
            certificate: cps[3]!.certificate,
            validators: fx.genesisSet,
          )) ==
          SkipResult.refused,
      'wrong set refused',
    );
    check(
      lc.acceptSkipping((
                header: cps[3]!.header,
                certificate: tampered(cps[3]!.certificate),
                validators: fx.newSet,
              )) ==
              SkipResult.refused &&
          lc.height == 0,
      'tampered cert refused',
    );
    var fetched = 0;
    lc.syncTo(cps[3]!, 3, (h) {
      fetched++;
      return cps[h];
    });
    check(
      lc.height == 3 &&
          fetched > 0 &&
          lc.validators().length == 2 &&
          lc.validators()[1].power == 3,
      'bisection sync',
    );
    final b4 = fx.b.propose(5);
    final c4 = fx.b.certify(b4);
    fx.b.submit(b4, c4);
    check(
      lc.accept(bc.blockHeader(b4), c4) && lc.height == 4,
      'sequential accept after a sync',
    );
    lc.free();
    fx.free();
    print('aviary dart light-client-skip smoke: OK');
  });
}
