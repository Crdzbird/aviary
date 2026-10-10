import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
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

class DartHashEngine extends ForeignEngine {
  int justified = 0, verified = 0;
  static Uint8List tag(Uint8List block) {
    final n = block.length < 16 ? block.length : 16;
    return Uint8List.fromList([
      ...'dart-just:'.codeUnits,
      ...block.sublist(0, n),
    ]);
  }

  @override
  Uint8List? justify(Uint8List block) {
    justified++;
    return tag(block);
  }

  @override
  bool verify(Uint8List block, Uint8List justification) {
    verified++;
    return eq(justification, tag(block));
  }
}

void main() {
  final bc = Aviary.instance;
  final eng = DartHashEngine();
  expect(bc.registerEngine('dart-engine', eng), 'register Dart engine');
  expect(
    bc.registerProposer('dart-first', (h, n) => 0),
    'register Dart proposer',
  );
  expect(
    bc.engineIds().contains('dart-engine') &&
        bc.proposerIds().contains('dart-first'),
    'discoverable',
  );

  final v = bc.newBLSValidator(fill(0x60));
  final genesis = bc.buildGenesis(
    chainId: 7,
    accounts: [(address: fill(0x11), balance: 1000)],
    validators: [(publicKey: v.public, power: 1)],
  );
  final chain = bc.assembleChain(
    bc.commitBlueprint(
      genesis,
      bc.makeBlueprint(consensus: 'dart-engine', stateModel: 'account'),
    ),
    0,
    v.public,
    v.secret,
  );
  expect(chain != null, 'Dart-engine chain assembles');
  final blk = chain!.propose(2);
  final just = chain.seal(blk);
  expect(
    String.fromCharCodes(just.sublist(0, 10)) == 'dart-just:',
    'seal via Dart justify',
  );
  final bad = Uint8List.fromList(just)..[just.length - 1] ^= 1;
  expect(
    !chain.submit(blk, bad) && chain.height == 0,
    'tampered justification refused, ledger untouched',
  );
  expect(
    chain.submit(blk, just) && chain.height == 1,
    'Dart-justified block committed',
  );
  expect(
    eng.justified >= 1 && eng.verified >= 2,
    'callbacks crossed the boundary',
  );
  chain.free();

  final pc = bc.assembleChain(
    bc.commitBlueprint(
      genesis,
      bc.makeBlueprint(
        consensus: 'pos',
        stateModel: 'account',
        proposer: 'dart-first',
      ),
    ),
    0,
    v.public,
    v.secret,
  )!;
  final pb = pc.propose(2);
  expect(
    pc.submit(pb, pc.seal(pb)) && pc.height == 1,
    'PoS with a Dart proposer committed',
  );
  pc.free();
  print(
    'aviary dart foreign-component smoke: OK (Dart engine + proposer in the mixture)',
  );
}
