import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';

void main() {
  test('observability', () async {
    final bc = Aviary.instance;
    final validator = bc.newValidator();
    final chain = bc.assembleSingleValidator(chainId: 9, validator: validator);
    final seen = <ChainEvent>[];
    final sub = chain.eventStream.listen(seen.add);
    chain.produceBlock(1);
    chain.produceBlock(2);
    await Future<void>.delayed(Duration.zero);
    check(seen.whereType<BlockFinalized>().length == 2, 'chain pushes blocks');
    check(chain.events().isEmpty, 'chain buffer empty while streaming');
    final m = chain.metricValues();
    check(
      m['aviary_chain_height'] == 2 && m['aviary_blocks_finalized_total'] == 2,
      'chain metrics',
    );
    check(chain.metrics().contains('# TYPE aviary_chain_height gauge'), 'text');
    check(!chain.submit(Uint8List.fromList([1, 2, 3])), 'garbage refused');
    check(
      chain.metricValues()['aviary_blocks_rejected_total'] == null,
      'undecodable blocks never reach the engine',
    );
    await sub.cancel();
    chain.free();

    final alice = Uint8List(32)..fillRange(0, 32, 0x11);
    final genesis = bc.buildGenesis(
      chainId: 42,
      accounts: [(address: alice, balance: 1000)],
      validators: [validator.entry(1)],
    );
    final net = bc.newNetNode(genesis: genesis, me: 0, blsKey: validator);
    net.start();
    check(net.committed, 'single validator commits on start');
    final events = net.events();
    check(events.whereType<BlockFinalized>().single.height == 1, 'net event');
    check(net.events().isEmpty, 'drain empties');
    net.onGossip(1, Uint8List.fromList([9, 9]));
    final nm = net.metricValues();
    check(
      nm['aviary_chain_height'] == 1 && nm['aviary_gossip_received_total'] == 1,
      'net metrics: $nm',
    );
    final pushed = <ChainEvent>[];
    final nsub = net.eventStream.listen(pushed.add);
    net.advance();
    await Future<void>.delayed(Duration.zero);
    check(pushed.whereType<BlockFinalized>().single.height == 2, 'net pushes');
    await nsub.cancel();
    net.free();
    print('aviary dart observability smoke: OK');
  });
}
