import 'dart:typed_data';
import 'package:blockchainer_dart/blockchainer.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool throws(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}

Uint8List fill(int b) => Uint8List(32)..fillRange(0, 32, b);

void main() {
  final bc = Blockchainer.instance;
  final va = bc.newBLSValidator(fill(0x91)),
      vb = bc.newBLSValidator(fill(0x92));
  final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');
  final genesisSet = [(publicKey: va.public, power: 1)];
  final gen = bc.commitBlueprint(
      bc.buildGenesis(chainId: 8, accounts: [], validators: genesisSet), bft);
  final gh = bc.genesisHeader(gen);
  final A = bc.assembleChain(gen, 0, va.public, va.secret)!,
      B = bc.assembleChain(gen, 0, vb.public, vb.secret)!;
  final cps = <int, Checkpoint>{};
  A.scheduleValidators(
      [(publicKey: va.public, power: 1), (publicKey: vb.public, power: 3)]);
  final b1 = A.propose(2), c1 = A.certify(b1);
  expect(A.submit(b1, c1) && B.submit(b1, c1), 'rotation block');
  cps[1] =
      (header: bc.blockHeader(b1), certificate: c1, validators: genesisSet);
  final newSet = B.validators();
  for (var ts = 3; ts <= 4; ts++) {
    final blk = B.propose(ts), cert = B.certify(blk);
    expect(B.submit(blk, cert) && A.submit(blk, cert), 'B block');
    cps[ts - 1] =
        (header: bc.blockHeader(blk), certificate: cert, validators: newSet);
  }
  final lc = bc.newLightClient(8, genesisSet, gh)!;
  expect(lc.acceptSkipping(cps[3]!) == SkipResult.insufficientOverlap,
      'no-overlap skip reports thin overlap');
  expect(
      lc.acceptSkipping((
            header: cps[3]!.header,
            certificate: cps[3]!.certificate,
            validators: genesisSet
          )) ==
          SkipResult.refused,
      'wrong set refused');
  final badCert = Uint8List.fromList(cps[3]!.certificate)
    ..[cps[3]!.certificate.length - 1] ^= 1;
  expect(
      lc.acceptSkipping((
                header: cps[3]!.header,
                certificate: badCert,
                validators: newSet
              )) ==
              SkipResult.refused &&
          lc.height == 0,
      'tampered cert refused');
  var fetched = 0;
  lc.syncTo(cps[3]!, 3, (h) {
    fetched++;
    return cps[h];
  });
  expect(
      lc.height == 3 &&
          fetched > 0 &&
          lc.validators().length == 2 &&
          lc.validators()[1].power == 3,
      'bisection sync');
  final b4 = B.propose(5), c4 = B.certify(b4);
  B.submit(b4, c4);
  expect(lc.accept(bc.blockHeader(b4), c4) && lc.height == 4,
      'sequential accept after a sync');
  final lc2 = bc.newLightClient(8, genesisSet, gh)!;
  lc2.setMaxSkip(1);
  expect(lc2.acceptSkipping(cps[2]!) == SkipResult.refused,
      'max_skip refuses a 2-block jump');
  lc2.setMaxSkip(0);
  expect(throws(() => lc2.syncTo(cps[3]!, 3, (_) => null)),
      'unavailable checkpoint fails');
  expect(bc.newLightClient(8, [], gh) == null, 'empty set refused');
  lc.free();
  lc2.free();
  A.free();
  B.free();
  print(
      'blockchainer dart light-client-skip smoke: OK (thin overlap -> bisection through the rotation -> follow the new set)');
}
