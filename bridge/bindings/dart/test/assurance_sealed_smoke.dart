import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';

void expect(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

bool fails(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}

void main() {
  final bc = Aviary.instance;

  final standard = bc.describeAssurance(1);
  expect(!standard.ratcheted && !standard.vrf,
      'standard: mutual transport, stake-weighted');
  expect(!standard.thresholdCustody && !standard.sealedMempool,
      'standard: no threshold, open mempool');

  final hardened = bc.describeAssurance(2);
  expect(hardened.ratcheted && hardened.vrf,
      'hardened: ratcheted transport, VRF consensus');
  expect(hardened.thresholdCustody && !hardened.sealedMempool,
      'hardened: threshold custody, open mempool');

  final fortress = bc.describeAssurance(3);
  expect(fortress.sealedMempool, 'fortress: seals the mempool');
  expect(fortress.thresholdCustody && fortress.ratcheted && fortress.vrf,
      'fortress: everything on');

  expect(fails(() => bc.describeAssurance(9)), 'unknown level fails closed');

  final proposer = bc.hybridKemKeypair();
  final pool = bc.makeSealedMempool(64);
  for (final fee in [50, 300, 120]) {
    final tx = bc.encodeTx(
      chainId: 7,
      fee: fee,
      body: Uint8List.fromList([1, 2, 3]),
      author: Uint8List(32),
      signature: Uint8List(64),
    );
    expect(pool.add(bc.sealTx(tx, proposer.public)),
        'sealed tx admitted (fee $fee)');
  }
  expect(pool.size == 3, 'pool holds 3 sealed txs');

  final revealed = pool.reveal(proposer.secret, 10);
  expect(revealed.length == 3, 'proposer reveals all 3 sealed txs');

  final stranger = bc.hybridKemKeypair();
  final strangerReveal = pool.reveal(stranger.secret, 10);
  expect(strangerReveal.isEmpty, 'a non-proposer opens nothing');

  pool.free();
  print('aviary dart assurance + sealed-mempool smoke: OK '
      '(3 tiers monotonic, 3 sealed txs, proposer-only reveal)');
}
