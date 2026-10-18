import 'package:flutter/material.dart';
import 'package:aviary_lite/aviary_lite.dart';

import 'network_page.dart';

/// A four validator full mesh driven one height at a time.
final class NetworkPageState extends State<NetworkPage> {
  late final PeerMesh _mesh = _build();
  var _started = false;

  PeerMesh _build() {
    final bc = Aviary.instance;
    final keys = List.generate(4, (_) => bc.newValidator());
    final genesis = bc.buildGenesis(
      chainId: 42,
      accounts: const [],
      validators: [for (final k in keys) k.entry(1)],
    );
    return PeerMesh([
      for (var i = 0; i < 4; i++)
        bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
    ])..linkAll();
  }

  void _next() {
    if (_started) {
      _mesh.advance();
      setState(_mesh.settle);
      return;
    }
    _started = true;
    _mesh.start();
    setState(_mesh.settle);
  }

  @override
  void dispose() {
    _mesh.free();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final nodes = _mesh.nodes;
    final tip = nodes.first.tip();
    final agree = nodes.every((n) => n.tip().equalsBytes(tip));
    return Column(
      children: [
        for (var i = 0; i < nodes.length; i++)
          ListTile(
            leading: CircleAvatar(child: Text('$i')),
            title: Text('Validator $i at height ${nodes[i].height}'),
            subtitle: Text('tip ${nodes[i].tip().shortHex}'),
          ),
        Text(agree ? 'Every validator holds the same tip.' : 'Tips differ.'),
        const SizedBox(height: 12),
        FilledButton(
          onPressed: _next,
          child: Text(_started ? 'Next height' : 'Start consensus'),
        ),
      ],
    );
  }
}
