import 'package:flutter/material.dart';
import 'package:blockchainer/blockchainer.dart';

/// Runs one ledger round trip and shows the result.
final class LedgerPage extends StatefulWidget {
  const LedgerPage({super.key});

  @override
  State<LedgerPage> createState() => _LedgerPageState();
}

final class _LedgerPageState extends State<LedgerPage> {
  String _status = 'Tap run to produce a block.';

  void _run() {
    final bc = Blockchainer.instance;
    final node = bc.createNode(7);
    final alice = node.newAccount();
    final bob = node.newAccount();
    node.credit(alice.address, 1000);
    node.submitTransfer(
      alice.secret,
      bob.address,
      250,
      node.nonce(alice.address),
    );
    final height = node.produceBlock(1);
    final proof = node.proveAccount(bob.address);
    final verified = bc.verifyStateProof(node.stateRoot(), proof);
    node.dispose();
    setState(
      () => _status =
          'blockchainer ${bc.version()}: height $height, bob ${node.balance(bob.address)}, proof $verified',
    );
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    appBar: AppBar(title: const Text('blockchainer')),
    body: Center(
      child: Column(
        mainAxisSize: MainAxisSize.min,
        children: [
          Padding(padding: const EdgeInsets.all(16), child: Text(_status)),
          FilledButton(onPressed: _run, child: const Text('Run')),
        ],
      ),
    ),
  );
}
