// Minimal blockchainer_flutter example.
//
// Shows the recommended library usage: initialize the native backend OFF the UI
// isolate (Blockchainer.ensureInitialized), then drive the ledger with the
// fully-synchronous API — no `await` on any ledger call.
import 'package:blockchainer_flutter/blockchainer_flutter.dart';
import 'package:flutter/material.dart';

void main() {
  // Isolate-backed warm-up, NOT awaited. The native libraries are already mapped
  // at app launch off the UI thread (Android ContentProvider / iOS dyld), so the
  // synchronous API below works immediately; preload() just pushes the one-time
  // crypto-backend init onto a background isolate. No await, no UI stall.
  // (Prefer `await Blockchainer.ensureInitialized()` only if you want to gate the
  //  UI on readiness.)
  Blockchainer.preload();
  runApp(const ExampleApp());
}

class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'blockchainer_flutter example',
        theme: ThemeData(colorSchemeSeed: Colors.indigo, useMaterial3: true),
        home: const LedgerScreen(),
      );
}

class LedgerScreen extends StatefulWidget {
  const LedgerScreen({super.key});
  @override
  State<LedgerScreen> createState() => _LedgerScreenState();
}

class _LedgerScreenState extends State<LedgerScreen> {
  // The backend is ready (main awaited ensureInitialized), so the API is fully
  // synchronous from here on — no `await` anywhere below.
  final Blockchainer _bc = Blockchainer.instance;
  late final Node _node;
  late final Account _alice;
  late final Account _bob;
  int _ts = 1;

  @override
  void initState() {
    super.initState();
    _node = _bc.createNode(1);
    _alice = _node.newAccount();
    _bob = _node.newAccount();
    _node.credit(_alice.address, 1000); // genesis credit (before the first block)
  }

  @override
  void dispose() {
    _node.dispose();
    super.dispose();
  }

  void _transfer() {
    _node.submitTransfer(_alice.secret, _bob.address, 100, _node.nonce(_alice.address));
    setState(() {});
  }

  void _produce() {
    _node.produceBlock(_ts++);
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text('Blockchainer ${_bc.version()}')),
      body: Padding(
        padding: const EdgeInsets.all(20),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text('crypto backend ${_bc.cryptoBackend()}',
                style: Theme.of(context).textTheme.bodySmall),
            const SizedBox(height: 16),
            _row('Height', '${_node.height()}'),
            _row('Mempool', '${_node.mempoolSize()}'),
            _row('Alice', '${_node.balance(_alice.address)}'),
            _row('Bob', '${_node.balance(_bob.address)}'),
            const Spacer(),
            Row(
              children: [
                Expanded(
                  child: FilledButton(
                    onPressed: _transfer,
                    child: const Text('Alice → Bob (100)'),
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: FilledButton.tonal(
                    onPressed: _produce,
                    child: const Text('Produce block'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  Widget _row(String k, String v) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Row(
          mainAxisAlignment: MainAxisAlignment.spaceBetween,
          children: [Text(k), Text(v, style: const TextStyle(fontWeight: FontWeight.bold))],
        ),
      );
}
