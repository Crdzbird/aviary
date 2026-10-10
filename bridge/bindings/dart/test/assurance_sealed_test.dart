import 'dart:typed_data';
import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

void check(bool ok, String msg) {
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
  test('assurance sealed', () {
    final bc = Aviary.instance;

    final standard = bc.describeAssurance(1);
    check(
      !standard.ratcheted && !standard.vrf,
      'standard: mutual transport, stake-weighted',
    );
    check(
      !standard.thresholdCustody && !standard.sealedMempool,
      'standard: no threshold, open mempool',
    );

    final hardened = bc.describeAssurance(2);
    check(
      hardened.ratcheted && hardened.vrf,
      'hardened: ratcheted transport, VRF consensus',
    );
    check(
      hardened.thresholdCustody && !hardened.sealedMempool,
      'hardened: threshold custody, open mempool',
    );

    final fortress = bc.describeAssurance(3);
    check(fortress.sealedMempool, 'fortress: seals the mempool');
    check(
      fortress.thresholdCustody && fortress.ratcheted && fortress.vrf,
      'fortress: everything on',
    );

    check(fails(() => bc.describeAssurance(9)), 'unknown level fails closed');

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
      check(
        pool.add(bc.sealTx(tx, proposer.public)),
        'sealed tx admitted (fee $fee)',
      );
    }
    check(pool.size == 3, 'pool holds 3 sealed txs');

    final revealed = pool.reveal(proposer.secret, 10);
    check(revealed.length == 3, 'proposer reveals all 3 sealed txs');

    final stranger = bc.hybridKemKeypair();
    final strangerReveal = pool.reveal(stranger.secret, 10);
    check(strangerReveal.isEmpty, 'a non-proposer opens nothing');

    pool.free();
    print(
      'aviary dart assurance + sealed-mempool smoke: OK '
      '(3 tiers monotonic, 3 sealed txs, proposer-only reveal)',
    );
  });
}
