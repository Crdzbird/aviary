import 'package:flutter/material.dart';
import 'package:aviary_lite/aviary_lite.dart';

import 'events_page.dart';

/// Listens to the node's push event stream; the stream closes when the node
/// is disposed, so there is no subscription to cancel.
final class EventsPageState extends State<EventsPage> {
  final _node = Aviary.instance.createNode(9);
  late final Account _alice = _node.fundAccount(1000000);
  late final Account _bob = _node.fundAccount(0);
  final _log = <String>[];

  @override
  void initState() {
    super.initState();
    _node.eventStream.listen(
      (event) => setState(() => _log.insert(0, _describe(event))),
    );
  }

  String _describe(ChainEvent event) => switch (event) {
    BlockFinalized(:final height, :final txCount) =>
      'block $height finalized, $txCount txs',
    TxAdmitted(:final txid) => 'admitted ${txid.shortHex}',
    TxRemoved(:final txid) => 'included ${txid.shortHex}',
    BlockRejected(:final attemptedHeight) => 'block $attemptedHeight rejected',
    UnknownEvent() => 'unknown event',
  };

  void _send() => _node.sendTo(_alice, _bob, 10);

  void _commit() => _node.commit();

  @override
  void dispose() {
    _node.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final metrics = _node.metricValues();
    final height = metrics['aviary_chain_height']?.toInt() ?? 0;
    final pending = metrics['aviary_mempool_size']?.toInt() ?? 0;
    return Column(
      children: [
        ListTile(
          leading: CircleAvatar(child: Text('$height')),
          title: Text('Height $height, $pending pending'),
          subtitle: Text('Bob holds ${_node.balance(_bob.address)}'),
        ),
        Wrap(
          spacing: 8,
          children: [
            FilledButton(onPressed: _send, child: const Text('Send 10')),
            OutlinedButton(
              onPressed: _commit,
              child: const Text('Commit block'),
            ),
          ],
        ),
        const Divider(),
        Expanded(
          child: ListView.builder(
            itemCount: _log.length,
            itemBuilder: (_, i) => ListTile(dense: true, title: Text(_log[i])),
          ),
        ),
      ],
    );
  }
}
