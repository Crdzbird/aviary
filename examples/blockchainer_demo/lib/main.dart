// Blockchainer demo — a Flutter dashboard driving the entire stack through the
// C ABI: create a chain, fund accounts, send transactions, produce blocks, watch
// a live event stream, read observability metrics, verify a light-client proof,
// and derive multi-chain interop addresses.
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:blockchainer_flutter/blockchainer_flutter.dart';
import 'package:flutter/material.dart';

void main() {
  // Isolate-backed warm-up, NOT awaited. preload() runs the one-time
  // crypto-backend init on a background isolate; we don't wait for it. The
  // synchronous API (Blockchainer.instance) is usable immediately:
  //   • the native libraries are already MAPPED at app launch off the UI thread
  //     — on Android by the BlockchainerStartup ContentProvider, on iOS/macOS by
  //     dyld loading the embedded framework — so the lazy load is cheap, and
  //   • preload() additionally pushes cryptolib_init off the UI isolate.
  // So: isolate loading, no await, no UI stall.
  Blockchainer.preload();
  runApp(const BlockchainerDemoApp());
}

Uint8List _unhex(String s) {
  final out = Uint8List(s.length ~/ 2);
  for (var i = 0; i < out.length; i++) {
    out[i] = int.parse(s.substring(i * 2, i * 2 + 2), radix: 16);
  }
  return out;
}

String _short(Uint8List b) =>
    b.map((x) => x.toRadixString(16).padLeft(2, '0')).join().substring(0, 12);

class BlockchainerDemoApp extends StatelessWidget {
  const BlockchainerDemoApp({super.key});
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Blockchainer',
      theme: ThemeData(
          colorSchemeSeed: const Color(0xFF6750A4), useMaterial3: true, brightness: Brightness.dark),
      home: const Dashboard(),
    );
  }
}

class Dashboard extends StatefulWidget {
  const Dashboard({super.key});
  @override
  State<Dashboard> createState() => _DashboardState();
}

class _DashboardState extends State<Dashboard> {
  Blockchainer? _bc;
  Node? _node;
  Account? _alice;
  Account? _bob;
  String? _error;

  int _height = 0;
  int _aliceBal = 0;
  int _bobBal = 0;
  int _mempool = 0;
  int _ts = 1;
  bool _proofOk = false;
  bool _autoMine = false;
  final List<Map<String, dynamic>> _events = [];
  Map<String, int> _metrics = {};
  Map<String, String> _interop = {};
  final _amountCtrl = TextEditingController(text: '100');
  Timer? _timer;

  @override
  void initState() {
    super.initState();
    _boot();
  }

  void _boot() {
    try {
      final bc = Blockchainer.instance;
      final node = bc.createNode(42);
      final alice = node.newAccount();
      final bob = node.newAccount();
      node.credit(alice.address, 1000);
      _bc = bc;
      _node = node;
      _alice = alice;
      _bob = bob;
      _interop = {
        'Ethereum': bc.ethAddress(
            _unhex('ac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80')),
        'Bitcoin': bc.btcAddress(
            _unhex('0000000000000000000000000000000000000000000000000000000000000001')),
        'Solana': bc.solAddress(alice.secret),
      };
      _refresh();
      _timer = Timer.periodic(const Duration(milliseconds: 800), (_) => _tick());
    } catch (e) {
      setState(() => _error = '$e');
    }
  }

  /// drainEvents() returns a JSON array string (same as every binding); parse it
  /// into structured events for the UI.
  List<Map<String, dynamic>> _drainEvents(Node node) =>
      (jsonDecode(node.drainEvents()) as List).cast<Map<String, dynamic>>();

  void _tick() {
    final node = _node;
    if (node == null) return;
    final newEvents = _drainEvents(node);
    if (_autoMine && node.mempoolSize() > 0) {
      node.produceBlock(_ts++);
      _refresh();
    } else if (newEvents.isNotEmpty) {
      _refresh(drained: newEvents);
    }
  }

  void _refresh({List<Map<String, dynamic>>? drained}) {
    final node = _node;
    if (node == null) return;
    final events = drained ?? _drainEvents(node);
    setState(() {
      _events.insertAll(0, events.reversed);
      if (_events.length > 50) _events.removeRange(50, _events.length);
      _height = node.height();
      _aliceBal = node.balance(_alice!.address);
      _bobBal = node.balance(_bob!.address);
      _mempool = node.mempoolSize();
      _metrics = _parseMetrics(node.metrics());
    });
  }

  Map<String, int> _parseMetrics(String text) {
    final m = <String, int>{};
    for (final line in text.split('\n')) {
      if (line.isEmpty || line.startsWith('#')) continue;
      final parts = line.split(' ');
      if (parts.length == 2) m[parts[0]] = int.tryParse(parts[1]) ?? 0;
    }
    return m;
  }

  void _send() {
    final node = _node;
    if (node == null) return;
    final amount = int.tryParse(_amountCtrl.text) ?? 0;
    if (amount <= 0) return;
    try {
      node.submitTransfer(_alice!.secret, _bob!.address, amount, node.nonce(_alice!.address));
      _refresh();
    } catch (e) {
      _snack('$e');
    }
  }

  void _produce() {
    final node = _node;
    if (node == null) return;
    try {
      node.produceBlock(_ts++);
      _refresh();
    } catch (e) {
      _snack('$e');
    }
  }

  void _verify() {
    final node = _node;
    if (node == null) return;
    final proof = node.proveAccount(_alice!.address);
    final ok = _bc!.verifyStateProof(node.stateRoot(), proof);
    setState(() => _proofOk = ok);
    _snack(ok ? 'Light-client proof of Alice verified ✓' : 'Proof failed');
  }

  void _snack(String msg) =>
      ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(msg)));

  @override
  void dispose() {
    _timer?.cancel();
    _node?.dispose();
    _amountCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_error != null) {
      return Scaffold(
          body: Center(child: Padding(padding: const EdgeInsets.all(24), child: Text(_error!))));
    }
    if (_bc == null) {
      return const Scaffold(body: Center(child: CircularProgressIndicator()));
    }
    return Scaffold(
      appBar: AppBar(
          title: Text('Blockchainer  ·  v${_bc!.version()}  ·  crypto ${_bc!.cryptoBackend()}')),
      body: ListView(
        padding: const EdgeInsets.all(16),
        children: [
          _chainCard(),
          _txCard(),
          _metricsCard(),
          _eventsCard(),
          _verifyCard(),
          _interopCard(),
        ],
      ),
    );
  }

  Widget _section(String title, IconData icon, List<Widget> children) {
    return Card(
      margin: const EdgeInsets.only(bottom: 16),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
          Row(children: [
            Icon(icon, size: 20),
            const SizedBox(width: 8),
            Expanded(
                child: Text(title,
                    style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold))),
          ]),
          const Divider(),
          ...children,
        ]),
      ),
    );
  }

  Widget _chainCard() => _section('Chain (id 42)', Icons.account_tree, [
        Wrap(spacing: 24, runSpacing: 8, children: [
          _stat('Height', '$_height'),
          _stat('Mempool', '$_mempool'),
          _stat('Alice', '$_aliceBal'),
          _stat('Bob', '$_bobBal'),
        ]),
        const SizedBox(height: 8),
        Text('alice ${_short(_alice!.address)}…   bob ${_short(_bob!.address)}…',
            style: const TextStyle(fontFamily: 'monospace', fontSize: 12, color: Colors.white54)),
      ]);

  Widget _txCard() => _section('Transactions & Consensus', Icons.swap_horiz, [
        Wrap(spacing: 12, runSpacing: 12, crossAxisAlignment: WrapCrossAlignment.center, children: [
          SizedBox(
              width: 110,
              child: TextField(
                  controller: _amountCtrl,
                  keyboardType: TextInputType.number,
                  decoration: const InputDecoration(labelText: 'amount'))),
          FilledButton.icon(
              onPressed: _send, icon: const Icon(Icons.send), label: const Text('Alice → Bob')),
          FilledButton.tonalIcon(
              onPressed: _produce, icon: const Icon(Icons.layers), label: const Text('Produce block')),
        ]),
        const SizedBox(height: 8),
        Row(children: [
          Switch(value: _autoMine, onChanged: (v) => setState(() => _autoMine = v)),
          const Flexible(child: Text('Auto-mine (finalize blocks as transactions arrive)')),
        ]),
      ]);

  Widget _metricsCard() => _section('Observability (Prometheus metrics)', Icons.insights, [
        Wrap(
            spacing: 24,
            runSpacing: 8,
            children: _metrics.entries
                .map((e) => _stat(e.key.replaceFirst('blockchainer_', ''), '${e.value}'))
                .toList()),
      ]);

  Widget _eventsCard() => _section('Live event stream', Icons.stream, [
        if (_events.isEmpty)
          const Text('No events yet — send a transaction.', style: TextStyle(color: Colors.white54))
        else
          ConstrainedBox(
            constraints: const BoxConstraints(maxHeight: 220),
            child: ListView(
              shrinkWrap: true,
              children: _events.map((e) {
                final type = e['type'] as String;
                final icon = type == 'block_finalized'
                    ? Icons.check_circle
                    : type == 'tx_admitted'
                        ? Icons.add_circle
                        : Icons.remove_circle;
                final color =
                    type == 'block_finalized' ? Colors.greenAccent : Colors.lightBlueAccent;
                return ListTile(
                  dense: true,
                  leading: Icon(icon, color: color, size: 18),
                  title: Text(type, style: const TextStyle(fontSize: 13)),
                  subtitle: Text(_eventDetail(e),
                      style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
                );
              }).toList(),
            ),
          ),
      ]);

  String _eventDetail(Map<String, dynamic> e) {
    switch (e['type']) {
      case 'block_finalized':
        return 'height ${e['height']} · ${e['tx_count']} tx · root ${(e['state_root'] as String).substring(0, 16)}…';
      case 'tx_admitted':
        return 'txid ${(e['txid'] as String).substring(0, 16)}… · mempool ${e['mempool_size']}';
      default:
        return 'mempool ${e['mempool_size']}';
    }
  }

  Widget _verifyCard() => _section('Verification (light client)', Icons.verified_user, [
        Wrap(spacing: 16, runSpacing: 8, crossAxisAlignment: WrapCrossAlignment.center, children: [
          FilledButton.icon(
              onPressed: _verify,
              icon: const Icon(Icons.fact_check),
              label: const Text('Prove & verify Alice')),
          if (_proofOk)
            const Row(mainAxisSize: MainAxisSize.min, children: [
              Icon(Icons.check_circle, color: Colors.greenAccent),
              SizedBox(width: 6),
              Text('State proof verified against the chain root'),
            ]),
        ]),
      ]);

  Widget _interopCard() => _section('Multi-chain interop addresses', Icons.hub, [
        ..._interop.entries.map((e) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 4),
              child: Row(children: [
                SizedBox(
                    width: 90, child: Text(e.key, style: const TextStyle(fontWeight: FontWeight.bold))),
                Expanded(
                    child: SelectableText(e.value,
                        style: const TextStyle(fontFamily: 'monospace', fontSize: 12))),
              ]),
            )),
      ]);

  Widget _stat(String label, String value) => Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: const TextStyle(fontSize: 12, color: Colors.white54)),
          Text(value, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
        ],
      );
}
