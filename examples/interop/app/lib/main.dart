// Interop client (Flutter side). Runs the Blockchainer ledger NATIVELY via
// blockchainer_flutter and cross-checks it against the Go server over gRPC, in
// three directions:
//   • go ↔ flutter : identical canonical bytes (BLAKE3 / txid / header hash)
//   • go → flutter : the client verifies a proof the GO node produced
//   • flutter → go : the GO node verifies a proof the CLIENT produced
//                    (+ both build the same scenario → state roots must match)
import 'dart:math';
import 'dart:typed_data';

import 'package:blockchainer_flutter/blockchainer_flutter.dart';
import 'package:fixnum/fixnum.dart';
import 'package:flutter/material.dart';
import 'package:grpc/grpc.dart';

import 'gen/interop.pbgrpc.dart' as pb;

void main() {
  Blockchainer.preload(); // background native warm-up, no await
  runApp(const InteropApp());
}

class InteropApp extends StatelessWidget {
  const InteropApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'Blockchainer Interop',
        theme: ThemeData(colorSchemeSeed: const Color(0xFF5E35B1), useMaterial3: true, brightness: Brightness.dark),
        home: const InteropScreen(),
      );
}

class _Check {
  _Check(this.label);
  final String label;
  bool? pass; // null = not run
  String detail = '';
}

class _Direction {
  _Direction(this.title, this.subtitle, this.icon, this.checks);
  final String title;
  final String subtitle;
  final IconData icon;
  final List<_Check> checks;
  bool get done => checks.every((c) => c.pass != null);
  bool get ok => checks.every((c) => c.pass == true);
}

class InteropScreen extends StatefulWidget {
  const InteropScreen({super.key});
  @override
  State<InteropScreen> createState() => _InteropScreenState();
}

class _InteropScreenState extends State<InteropScreen> {
  final _rng = Random();
  final _host = TextEditingController(text: '127.0.0.1');
  ClientChannel? _channel;
  pb.InteropClient? _client;
  String? _error;
  bool _running = false;
  String _serverInfo = '';

  late final List<_Direction> _dirs = [
    _Direction('go <-> flutter', 'Canonical bytes are byte-identical', Icons.sync_alt, [
      _Check('BLAKE3 hash matches'),
      _Check('canonical txid matches'),
      _Check('header hash matches'),
    ]),
    _Direction('go -> flutter', "Client verifies the Go node's proof", Icons.south_west, [
      _Check('state roots match (determinism)'),
      _Check('client verifies Go proof'),
    ]),
    _Direction('flutter -> go', "Go node verifies the client's proof", Icons.north_east, [
      _Check('Go verifies client proof'),
    ]),
  ];

  @override
  void initState() {
    super.initState();
    _connectAndRun();
  }

  Future<void> _connectAndRun() async {
    setState(() => _error = null);
    try {
      _channel = ClientChannel(_host.text.trim(),
          port: 50053, options: const ChannelOptions(credentials: ChannelCredentials.insecure()));
      _client = pb.InteropClient(_channel!);
      await _runAll();
    } catch (e) {
      setState(() => _error = '$e');
    }
  }

  Uint8List _rand(int n) {
    final b = Uint8List(n);
    for (var i = 0; i < n; i++) {
      b[i] = _rng.nextInt(256);
    }
    return b;
  }

  bool _eq(List<int> a, List<int> b) {
    if (a.length != b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] != b[i]) return false;
    }
    return true;
  }

  String _hex(List<int> b, [int n = 8]) =>
      b.take(n).map((x) => x.toRadixString(16).padLeft(2, '0')).join();

  Future<void> _runAll() async {
    setState(() {
      _running = true;
      for (final d in _dirs) {
        for (final c in d.checks) {
          c.pass = null;
          c.detail = '';
        }
      }
    });
    try {
      await _runCanonical();
      await _runLedger();
    } catch (e) {
      setState(() => _error = '$e');
    } finally {
      if (mounted) setState(() => _running = false);
    }
  }

  // go <-> flutter: compute canonical bytes both sides for identical inputs.
  Future<void> _runCanonical() async {
    final bc = Blockchainer.instance;
    final msg = _rand(24);
    final body = Uint8List.fromList([0, 1, 2, 3]);
    final author = _rand(32), sig = _rand(64);
    final parent = _rand(32), txRoot = _rand(32), stateRoot = _rand(32), consensus = _rand(6);

    final localHash = bc.hash(msg);
    final localTxid = bc.txid(3, 7, body, author, sig);
    final localHdr = bc.headerHash(2, 7, 5, parent, 123, txRoot, stateRoot, consensus);

    final co = await _client!.computeCanonical(pb.CanonicalInput()
      ..message = msg
      ..txVersion = 3
      ..chainId = Int64(7)
      ..txBody = body
      ..txAuthor = author
      ..txSignature = sig
      ..hdrVersion = 2
      ..height = Int64(5)
      ..parentHash = parent
      ..timestamp = Int64(123)
      ..txRoot = txRoot
      ..stateRoot = stateRoot
      ..consensus = consensus);

    final d = _dirs[0];
    _set(d.checks[0], _eq(localHash, co.hash), 'flutter ${_hex(localHash)}…  go ${_hex(co.hash)}…');
    _set(d.checks[1], _eq(localTxid, co.txid), 'flutter ${_hex(localTxid)}…  go ${_hex(co.txid)}…');
    _set(d.checks[2], _eq(localHdr, co.headerHash), 'flutter ${_hex(localHdr)}…  go ${_hex(co.headerHash)}…');
  }

  // go -> flutter + flutter -> go: replicated ledger + bidirectional proofs.
  Future<void> _runLedger() async {
    final bc = Blockchainer.instance;
    final node = bc.createNode(7);
    final alice = node.newAccount();
    final bob = node.newAccount();

    // Go builds the scenario from the shared secrets and returns a proof.
    final res = await _client!.buildChain(pb.ChainScenario()
      ..chainId = Int64(7)
      ..aliceSecret = alice.secret
      ..bobSecret = bob.secret
      ..aliceCredit = Int64(1000000)
      ..transferAmount = Int64(250000)
      ..timestamp = Int64(1));

    // Flutter applies the IDENTICAL scenario locally.
    node.credit(alice.address, 1000000);
    node.submitTransfer(alice.secret, bob.address, 250000, node.nonce(alice.address));
    node.produceBlock(1);
    final localRoot = node.stateRoot();
    final localProof = node.proveAccount(bob.address);

    final goRoot = Uint8List.fromList(res.stateRoot);
    final goProof = Uint8List.fromList(res.proof);

    final d1 = _dirs[1];
    _set(d1.checks[0], _eq(localRoot, goRoot),
        'flutter ${_hex(localRoot)}…  go ${_hex(goRoot)}…  (bob=${res.bobBalance})');
    _set(d1.checks[1], bc.verifyStateProof(goRoot, goProof), '${goProof.length}-byte proof from Go verified locally');

    // Go verifies the Flutter-produced proof.
    final vr = await _client!.verifyProof(pb.ProofBundle()
      ..stateRoot = localRoot
      ..proof = localProof);
    _set(_dirs[2].checks[0], vr.ok, '${localProof.length}-byte proof from Flutter verified by Go');

    _serverInfo = 'chain 7 · height ${res.height} · alice ${res.aliceBalance} · bob ${res.bobBalance}';
    node.dispose();
  }

  void _set(_Check c, bool pass, String detail) {
    setState(() {
      c.pass = pass;
      c.detail = detail;
    });
  }

  @override
  void dispose() {
    _channel?.shutdown();
    _host.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final allDone = _dirs.every((d) => d.done);
    final allOk = allDone && _dirs.every((d) => d.ok);
    return Scaffold(
      appBar: AppBar(
        title: const Text('Blockchainer · Go <-> Flutter'),
        actions: [
          IconButton(onPressed: _running ? null : _runAll, icon: const Icon(Icons.refresh)),
        ],
      ),
      body: ListView(padding: const EdgeInsets.all(16), children: [
        Card(
          color: !allDone
              ? null
              : allOk
                  ? Colors.green.withValues(alpha: 0.18)
                  : Colors.red.withValues(alpha: 0.18),
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Row(children: [
              Icon(
                  !allDone
                      ? Icons.hourglass_top
                      : allOk
                          ? Icons.verified
                          : Icons.error,
                  size: 32,
                  color: allOk ? Colors.greenAccent : (allDone ? Colors.redAccent : null)),
              const SizedBox(width: 12),
              Expanded(
                child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                  Text(
                      !allDone
                          ? 'Running cross-binding checks…'
                          : allOk
                              ? 'Ledger is byte-identical across Go and Flutter'
                              : 'MISMATCH — bindings disagree',
                      style: const TextStyle(fontSize: 16, fontWeight: FontWeight.bold)),
                  if (_serverInfo.isNotEmpty)
                    Text('Blockchainer ${Blockchainer.instance.version()} · $_serverInfo',
                        style: const TextStyle(fontSize: 12, color: Colors.white60)),
                ]),
              ),
            ]),
          ),
        ),
        if (_error != null)
          Card(
            color: Colors.red.withValues(alpha: 0.15),
            child: Padding(
              padding: const EdgeInsets.all(16),
              child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                Text('Cannot reach the Go interop server.\n$_error'),
                const SizedBox(height: 8),
                const Text('Start it: cd examples/interop/server && go run .  (port 50053)',
                    style: TextStyle(color: Colors.white54, fontSize: 12)),
                const SizedBox(height: 8),
                FilledButton(onPressed: _connectAndRun, child: const Text('Retry')),
              ]),
            ),
          ),
        ..._dirs.map(_directionCard),
      ]),
    );
  }

  Widget _directionCard(_Direction d) => Card(
        margin: const EdgeInsets.only(top: 12),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              Icon(d.icon, size: 20),
              const SizedBox(width: 8),
              Expanded(
                child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
                  Text(d.title, style: const TextStyle(fontSize: 17, fontWeight: FontWeight.bold, fontFamily: 'monospace')),
                  Text(d.subtitle, style: const TextStyle(fontSize: 12, color: Colors.white54)),
                ]),
              ),
              if (d.done)
                Icon(d.ok ? Icons.check_circle : Icons.cancel,
                    color: d.ok ? Colors.greenAccent : Colors.redAccent),
            ]),
            const Divider(),
            ...d.checks.map(_checkRow),
          ]),
        ),
      );

  Widget _checkRow(_Check c) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 4),
        child: Row(crossAxisAlignment: CrossAxisAlignment.start, children: [
          SizedBox(
            width: 22,
            child: c.pass == null
                ? const SizedBox(width: 14, height: 14, child: CircularProgressIndicator(strokeWidth: 2))
                : Icon(c.pass! ? Icons.check : Icons.close,
                    size: 18, color: c.pass! ? Colors.greenAccent : Colors.redAccent),
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
              Text(c.label, style: const TextStyle(fontSize: 13)),
              if (c.detail.isNotEmpty)
                Text(c.detail, style: const TextStyle(fontFamily: 'monospace', fontSize: 11, color: Colors.white54)),
            ]),
          ),
        ]),
      );
}
