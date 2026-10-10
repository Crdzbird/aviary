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
bool eq(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

void main() {
  final bc = Blockchainer.instance;
  final me = bc.newBLSValidator(fill(0x81)),
      other = bc.newBLSValidator(fill(0x82));
  final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');
  final chain = bc.assembleChain(
      bc.commitBlueprint(
          bc.buildGenesis(
              chainId: 5,
              accounts: [],
              validators: [(publicKey: me.public, power: 1)]),
          bft),
      0,
      me.public,
      me.secret)!;
  expect(
      chain.validators().length == 1 &&
          eq(chain.validators()[0].publicKey, me.public),
      'initial set');
  final grown = [
    (publicKey: me.public, power: 3),
    (publicKey: other.public, power: 1)
  ];
  chain.scheduleValidators(grown);
  final b1 = chain.propose(2);
  expect(chain.submit(b1, chain.certify(b1)), 'rotation block');
  final vs = chain.validators();
  expect(
      vs.length == 2 && vs[0].power == 3 && eq(vs[1].publicKey, other.public),
      'set after rotation');
  final b2 = chain.propose(3);
  expect(chain.submit(b2, chain.certify(b2)) && chain.height == 2,
      'block under the new set');
  expect(
      throws(() => chain.scheduleValidators([])) &&
          throws(() =>
              chain.scheduleValidators([(publicKey: me.public, power: 0)])),
      'malformed sets refused');
  chain.scheduleValidators([(publicKey: other.public, power: 1)]);
  final b3 = chain.propose(4);
  expect(chain.submit(b3, chain.certify(b3)) && chain.height == 3,
      'leaving block');
  expect(throws(() => chain.certify(chain.propose(5))),
      'a node that left cannot certify');
  final pc = bc.assembleChain(
      bc.commitBlueprint(
          bc.buildGenesis(
              chainId: 6,
              accounts: [],
              validators: [(publicKey: me.public, power: 1)]),
          bc.makeBlueprint(
              consensus: 'pos',
              stateModel: 'account',
              proposer: 'round-robin')),
      0,
      me.public,
      me.secret)!;
  expect(throws(() => pc.scheduleValidators(grown)), 'PoS refuses');
  chain.free();
  pc.free();
  print(
      'blockchainer dart validator-rotation smoke: OK (grow, certify under each set, rotate out, PoS refuses)');
}
