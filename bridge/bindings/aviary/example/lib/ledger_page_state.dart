import 'package:flutter/material.dart';
import 'package:aviary/aviary.dart';

import 'ledger_page.dart';

/// Runs the round trip on a fresh node and shows what happened.
final class LedgerPageState extends State<LedgerPage> {
  String _status = 'Tap run to fund two accounts, transfer and finalize.';

  void _run() {
    final bc = Aviary.instance;
    final node = bc.createNode(7);
    final alice = node.fundAccount(1000);
    final bob = node.fundAccount(0);
    node.sendTo(alice, bob, 250);
    final height = node.commit();
    final bobBalance = node.balance(bob.address);
    final verified = node.verifyAccountProof(node.proveAccount(bob.address));
    final root = node.stateRoot().shortHex;
    node.dispose();
    setState(
      () => _status =
          'aviary ${bc.version()}\nheight $height, bob holds $bobBalance\n'
          'state root $root\nbalance proof verifies: $verified',
    );
  }

  @override
  Widget build(BuildContext context) => Center(
    child: Column(
      mainAxisSize: MainAxisSize.min,
      children: [
        Padding(
          padding: const EdgeInsets.all(16),
          child: Text(_status, textAlign: TextAlign.center),
        ),
        FilledButton(onPressed: _run, child: const Text('Run')),
      ],
    ),
  );
}
