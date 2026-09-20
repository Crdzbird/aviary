// Atlas Settlement Network — Flutter operator console. A thin gRPC client (no
// native blockchain code): all chain work happens in the Go server, which uses
// Blockchainer. Demonstrates custom settlement chains, institutional custodial
// wallets (multi-chain), instant-finality settlement with cryptographic balance
// proofs, EVM smart-contract signing, and a live ledger event stream.
import 'dart:async';

import 'package:fixnum/fixnum.dart';
import 'package:flutter/material.dart';
import 'package:grpc/grpc.dart';

import 'gen/atlas.pbgrpc.dart' as pb;

const int kChainId = 701;

void main() => runApp(const AtlasApp());

class AtlasApp extends StatelessWidget {
  const AtlasApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'Atlas Settlement Network',
        theme: ThemeData(colorSchemeSeed: const Color(0xFF1565C0), useMaterial3: true, brightness: Brightness.dark),
        home: const Console(),
      );
}

class Console extends StatefulWidget {
  const Console({super.key});
  @override
  State<Console> createState() => _ConsoleState();
}

class _ConsoleState extends State<Console> {
  final _hostCtrl = TextEditingController(text: 'localhost');
  pb.AtlasClient? _client;
  ClientChannel? _channel;
  String? _error;

  pb.Chain? _chain;
  List<pb.Wallet> _wallets = [];
  pb.Metrics? _metrics;
  final List<pb.Event> _events = [];
  StreamSubscription<pb.Event>? _eventSub;
  String? _fromId, _toId;
  String _lastSigned = '';
  String _lastProof = '';
  final _amountCtrl = TextEditingController(text: '500000');
  pb.Council? _council;
  bool _councilBusy = false;

  @override
  void initState() {
    super.initState();
    _connect();
  }

  Future<void> _connect() async {
    setState(() => _error = null);
    try {
      _channel = ClientChannel(_hostCtrl.text.trim(),
          port: 50051,
          options: const ChannelOptions(credentials: ChannelCredentials.insecure()));
      _client = pb.AtlasClient(_channel!);
      await _bootstrap();
      _startStream();
    } catch (e) {
      setState(() => _error = '$e');
    }
  }

  // Ensure a demo settlement chain with a few institutions exists.
  Future<void> _bootstrap() async {
    final list = await _client!.listChains(pb.Empty());
    pb.Chain? chain = list.chains.where((c) => c.chainId.toInt() == kChainId).firstOrNull;
    if (chain == null) {
      await _client!.createChain(pb.CreateChainRequest()
        ..name = 'Atlas EUR Settlement'
        ..chainId = Int64(kChainId)
        ..nativeSymbol = 'EURx');
      final inst = {
        'Treasury Ops': 'Banco Atlas',
        'Settlement Desk': 'Meridian Capital',
        'Bridge Custody': 'Atlas Bridge',
      };
      for (final e in inst.entries) {
        await _client!.createWallet(pb.CreateWalletRequest()
          ..chainId = Int64(kChainId)
          ..label = e.key
          ..institution = e.value);
      }
      final ws = (await _client!.listWallets(pb.ChainRef()..chainId = Int64(kChainId))).wallets;
      for (var i = 0; i < ws.length; i++) {
        await _client!.issueFunds(pb.IssueRequest()
          ..chainId = Int64(kChainId)
          ..walletId = ws[i].id
          ..amount = Int64([5000000, 2000000, 1000000][i % 3]));
      }
      await _client!.produceBlock(pb.ChainRef()..chainId = Int64(kChainId));
    }
    await _refresh();
  }

  Future<void> _refresh() async {
    final ref = pb.ChainRef()..chainId = Int64(kChainId);
    final chains = await _client!.listChains(pb.Empty());
    final wallets = await _client!.listWallets(ref);
    final metrics = await _client!.getMetrics(ref);
    if (!mounted) return;
    setState(() {
      _chain = chains.chains.where((c) => c.chainId.toInt() == kChainId).firstOrNull;
      _wallets = wallets.wallets;
      _metrics = metrics;
      _fromId ??= _wallets.isNotEmpty ? _wallets[0].id : null;
      _toId ??= _wallets.length > 1 ? _wallets[1].id : null;
    });
  }

  void _startStream() {
    _eventSub?.cancel();
    final ref = pb.ChainRef()..chainId = Int64(kChainId);
    _eventSub = _client!.streamEvents(ref).listen((ev) {
      setState(() {
        _events.insert(0, ev);
        if (_events.length > 40) _events.removeLast();
      });
      _refresh();
    }, onError: (_) {});
  }

  int get _amount => int.tryParse(_amountCtrl.text) ?? 0;

  Future<void> _settle() async {
    if (_fromId == null || _toId == null || _fromId == _toId || _amount <= 0) return;
    await _guard(() => _client!.settle(pb.SettleRequest()
      ..chainId = Int64(kChainId)
      ..fromWalletId = _fromId!
      ..toWalletId = _toId!
      ..amount = Int64(_amount)
      ..memo = 'console settlement'));
    _snack('Settlement queued — produce a block to finalize');
  }

  Future<void> _produce() async {
    final b = await _guard(() => _client!.produceBlock(pb.ChainRef()..chainId = Int64(kChainId)));
    if (b != null) _snack('Block ${b.height} finalized');
    await _refresh();
  }

  Future<void> _issue(String walletId) async {
    await _guard(() => _client!.issueFunds(pb.IssueRequest()
      ..chainId = Int64(kChainId)
      ..walletId = walletId
      ..amount = Int64(1000000)));
    _snack('Issued 1,000,000 — produce a block to finalize');
  }

  Future<void> _proof(String walletId) async {
    final p = await _guard(() => _client!.getBalanceProof(pb.WalletRef()
      ..chainId = Int64(kChainId)
      ..walletId = walletId));
    if (p != null) {
      setState(() => _lastProof = 'balance ${p.balance} · ${p.proofBytes}-byte proof · verified ${p.verified}');
    }
  }

  Future<void> _erc20(pb.Wallet w) async {
    final tx = await _guard(() => _client!.buildErc20Transfer(pb.Erc20Request()
      ..chainId = Int64(kChainId)
      ..walletId = w.id
      ..tokenAddress = '0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48' // USDC
      ..toAddress = _wallets.firstWhere((x) => x.id != w.id, orElse: () => w).ethAddress
      ..amount = Int64(250000000)
      ..nonce = Int64.ZERO
      ..gasPrice = Int64(30000000000)
      ..gasLimit = Int64(60000)
      ..evmChainId = Int64.ONE));
    if (tx != null) setState(() => _lastSigned = tx.rawTxHex);
  }

  Future<T?> _guard<T>(Future<T> Function() f) async {
    try {
      return await f();
    } catch (e) {
      _snack('$e');
      return null;
    }
  }

  void _snack(String m) => ScaffoldMessenger.of(context).showSnackBar(SnackBar(content: Text(m)));

  @override
  void dispose() {
    _eventSub?.cancel();
    _channel?.shutdown();
    _amountCtrl.dispose();
    _hostCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    if (_client == null || _chain == null) {
      return Scaffold(
        appBar: AppBar(title: const Text('Atlas Settlement Network')),
        body: Center(
          child: _error != null
              ? Padding(
                  padding: const EdgeInsets.all(24),
                  child: Column(mainAxisSize: MainAxisSize.min, children: [
                    Text('Cannot reach the Atlas server.\n$_error', textAlign: TextAlign.center),
                    const SizedBox(height: 8),
                    const Text('Start it: cd examples/atlas/server && go run .',
                        style: TextStyle(color: Colors.white54)),
                    const SizedBox(height: 16),
                    FilledButton(onPressed: _connect, child: const Text('Retry')),
                  ]))
              : const CircularProgressIndicator(),
        ),
      );
    }
    final c = _chain!;
    return DefaultTabController(
      length: 2,
      child: Scaffold(
        appBar: AppBar(
          title: Text('Atlas · ${c.name}'),
          actions: [
            Center(child: Text('${c.nativeSymbol}  ', style: const TextStyle(fontWeight: FontWeight.bold))),
            IconButton(onPressed: _refresh, icon: const Icon(Icons.refresh)),
          ],
          bottom: const TabBar(tabs: [
            Tab(icon: Icon(Icons.account_balance), text: 'Settlement'),
            Tab(icon: Icon(Icons.how_to_vote), text: 'Governance'),
          ]),
        ),
        body: TabBarView(children: [
          ListView(padding: const EdgeInsets.all(16), children: [
            _chainCard(c),
            _walletsCard(),
            _settleCard(),
            _metricsCard(),
            _eventsCard(),
            _contractCard(),
          ]),
          _governanceTab(),
        ]),
      ),
    );
  }

  // ── Governance tab: validator council rotation + auditor light client ──────
  Future<void> _councilOp(pb.CouncilOp? op) async {
    setState(() => _councilBusy = true);
    try {
      final ref = pb.ChainRef()..chainId = Int64(kChainId);
      final c = op == null
          ? await _client!.getCouncil(ref)
          : await _client!.councilAction(pb.CouncilRequest()..chainId = Int64(kChainId)..op = op);
      if (mounted) setState(() => _council = c);
    } catch (e) {
      if (mounted) setState(() => _error = '$e');
    } finally {
      if (mounted) setState(() => _councilBusy = false);
    }
  }

  Widget _governanceTab() {
    final c = _council;
    if (c == null && !_councilBusy) _councilOp(null);
    return ListView(padding: const EdgeInsets.all(16), children: [
      _section('Validator council', Icons.how_to_vote, [
        const Text(
            'The settlement rail is governed by a BFT validator council. A block may '
            'ANNOUNCE the next set; it is certified by the current set, so the change is '
            'itself finalized. The operator schedules a rotation, the next block carries it, '
            'and from then on the new set rules.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        if (c != null)
          Wrap(spacing: 28, runSpacing: 10, children: [
            _stat('Council height', '${c.height}'),
            _stat('Validators', '${c.validators.length}'),
            _stat('Rotations', '${c.rotations}'),
          ]),
        const SizedBox(height: 8),
        if (c != null)
          for (final v in c.validators)
            Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Row(children: [
                Icon(v.operator ? Icons.star : Icons.person, size: 16, color: v.operator ? Colors.amberAccent : Colors.white54),
                const SizedBox(width: 6),
                Expanded(child: Text('${v.publicKey.substring(0, 16)}…  power ${v.power}${v.operator ? "  (operator)" : ""}',
                    style: const TextStyle(fontFamily: 'monospace', fontSize: 12))),
              ]),
            ),
        const SizedBox(height: 10),
        Wrap(spacing: 8, runSpacing: 8, children: [
          FilledButton.icon(onPressed: _councilBusy ? null : () => _councilOp(pb.CouncilOp.COUNCIL_PRODUCE), icon: const Icon(Icons.layers), label: const Text('Produce block')),
          FilledButton.icon(onPressed: _councilBusy ? null : () => _councilOp(pb.CouncilOp.COUNCIL_ADD_VALIDATOR), icon: const Icon(Icons.person_add), label: const Text('Add validator (rotate)')),
        ]),
      ]),
      _section('Auditor (light client with the overlap rule)', Icons.travel_explore, [
        const Text(
            'An auditor that only ever trusted the GENESIS set catches up without replaying '
            'every header: a far header is accepted when its own set certifies it and the '
            'signers hold more than 1/3 of the set the auditor trusts. Across rotations the '
            'overlap is too thin, so it bisects through the rotation headers.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        if (c != null) _stat('Auditor trusted height', '${c.auditorHeight} / ${c.height}'),
        const SizedBox(height: 10),
        FilledButton.icon(onPressed: _councilBusy ? null : () => _councilOp(pb.CouncilOp.COUNCIL_AUDITOR_SYNC), icon: const Icon(Icons.sync), label: const Text('Sync auditor from genesis')),
      ]),
      _section('Governance log', Icons.receipt_long, [
        if (c == null || c.log.isEmpty)
          const Text('No governance activity yet.', style: TextStyle(color: Colors.white54))
        else
          for (final s in c.log.reversed)
            ListTile(
              dense: true,
              contentPadding: EdgeInsets.zero,
              leading: Icon(s.ok ? Icons.check_circle : Icons.error, color: s.ok ? Colors.lightGreenAccent : Colors.redAccent, size: 18),
              title: Text(s.label, style: const TextStyle(fontSize: 13)),
              subtitle: Text(s.detail, style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
            ),
      ]),
    ]);
  }

  Widget _section(String title, IconData icon, List<Widget> children) => Card(
        margin: const EdgeInsets.only(bottom: 16),
        child: Padding(
          padding: const EdgeInsets.all(16),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
            Row(children: [
              Icon(icon, size: 20),
              const SizedBox(width: 8),
              Expanded(child: Text(title, style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold))),
            ]),
            const Divider(),
            ...children,
          ]),
        ),
      );

  Widget _chainCard(pb.Chain c) => _section('Settlement chain', Icons.account_balance, [
        Wrap(spacing: 28, runSpacing: 10, children: [
          _stat('Height', '${c.height}'),
          _stat('Institutions', '${c.walletCount}'),
          _stat('Issued ${c.nativeSymbol}', _fmt(c.totalIssued.toInt())),
        ]),
        const SizedBox(height: 12),
        FilledButton.icon(onPressed: _produce, icon: const Icon(Icons.layers), label: const Text('Produce block (finalize batch)')),
      ]);

  Widget _walletsCard() => _section('Institutional wallets', Icons.account_balance_wallet, [
        for (final w in _wallets) _walletTile(w),
      ]);

  Widget _walletTile(pb.Wallet w) => ExpansionTile(
        tilePadding: EdgeInsets.zero,
        title: Text('${w.institution} · ${w.label}'),
        subtitle: Text('${_fmt(w.balance.toInt())} ${_chain!.nativeSymbol}',
            style: const TextStyle(fontWeight: FontWeight.bold, color: Colors.lightGreenAccent)),
        children: [
          _addrRow('Native', w.address),
          _addrRow('Ethereum', w.ethAddress),
          _addrRow('Bitcoin', w.btcAddress),
          _addrRow('Solana', w.solAddress),
          Padding(
            padding: const EdgeInsets.symmetric(vertical: 8),
            child: Wrap(spacing: 8, children: [
              OutlinedButton(onPressed: () => _issue(w.id), child: const Text('Issue 1M')),
              OutlinedButton(onPressed: () => _proof(w.id), child: const Text('Prove balance')),
              OutlinedButton(onPressed: () => _erc20(w), child: const Text('Sign USDC tx')),
            ]),
          ),
        ],
      );

  Widget _addrRow(String chain, String addr) => Padding(
        padding: const EdgeInsets.symmetric(vertical: 2),
        child: Row(children: [
          SizedBox(width: 80, child: Text(chain, style: const TextStyle(color: Colors.white54))),
          Expanded(child: SelectableText(addr, style: const TextStyle(fontFamily: 'monospace', fontSize: 11))),
        ]),
      );

  Widget _settleCard() => _section('Settle', Icons.swap_horiz, [
        Wrap(spacing: 12, runSpacing: 12, crossAxisAlignment: WrapCrossAlignment.center, children: [
          _walletDrop(_fromId, (v) => setState(() => _fromId = v)),
          const Icon(Icons.arrow_forward),
          _walletDrop(_toId, (v) => setState(() => _toId = v)),
          SizedBox(width: 130, child: TextField(controller: _amountCtrl, keyboardType: TextInputType.number, decoration: const InputDecoration(labelText: 'amount'))),
          FilledButton.icon(onPressed: _settle, icon: const Icon(Icons.send), label: const Text('Settle')),
        ]),
      ]);

  Widget _walletDrop(String? value, ValueChanged<String?> onChanged) => DropdownButton<String>(
        value: value,
        items: _wallets.map((w) => DropdownMenuItem(value: w.id, child: Text(w.institution))).toList(),
        onChanged: onChanged,
      );

  Widget _metricsCard() => _section('Observability', Icons.insights, [
        if (_metrics != null)
          Wrap(spacing: 28, runSpacing: 10, children: [
            _stat('Height', '${_metrics!.height}'),
            _stat('Blocks', '${_metrics!.blocks}'),
            _stat('Transactions', '${_metrics!.transactions}'),
            _stat('Mempool', '${_metrics!.mempool}'),
          ]),
        if (_lastProof.isNotEmpty)
          Padding(
              padding: const EdgeInsets.only(top: 10),
              child: Row(children: [
                const Icon(Icons.verified_user, color: Colors.lightGreenAccent, size: 18),
                const SizedBox(width: 6),
                Flexible(child: Text('Light-client proof: $_lastProof')),
              ])),
      ]);

  Widget _eventsCard() => _section('Live ledger feed', Icons.stream, [
        if (_events.isEmpty)
          const Text('Awaiting events — settle or issue funds.', style: TextStyle(color: Colors.white54))
        else
          ConstrainedBox(
            constraints: const BoxConstraints(maxHeight: 200),
            child: ListView(shrinkWrap: true, children: _events.map((e) {
              final isBlock = e.type == pb.EventType.BLOCK_FINALIZED;
              return ListTile(
                dense: true,
                leading: Icon(isBlock ? Icons.check_circle : Icons.bolt,
                    color: isBlock ? Colors.lightGreenAccent : Colors.amberAccent, size: 18),
                title: Text(e.type.name, style: const TextStyle(fontSize: 13)),
                subtitle: Text(
                    isBlock
                        ? 'block ${e.height} · ${e.txCount} tx'
                        : 'mempool ${e.mempoolSize}${e.txid.isNotEmpty ? " · ${e.txid.substring(0, 12)}…" : ""}',
                    style: const TextStyle(fontFamily: 'monospace', fontSize: 11)),
              );
            }).toList()),
          ),
      ]);

  Widget _contractCard() => _section('EVM smart contract (signed, ready to broadcast)', Icons.code, [
        const Text('Tap "Sign USDC tx" on a wallet to build + sign a real ERC-20 transfer.',
            style: TextStyle(color: Colors.white54)),
        if (_lastSigned.isNotEmpty)
          Padding(
            padding: const EdgeInsets.only(top: 10),
            child: SelectableText(_lastSigned,
                style: const TextStyle(fontFamily: 'monospace', fontSize: 11, color: Colors.cyanAccent)),
          ),
      ]);

  Widget _stat(String label, String value) => Column(crossAxisAlignment: CrossAxisAlignment.start, children: [
        Text(label, style: const TextStyle(fontSize: 12, color: Colors.white54)),
        Text(value, style: const TextStyle(fontSize: 22, fontWeight: FontWeight.bold)),
      ]);

  String _fmt(int v) {
    final s = v.toString();
    final b = StringBuffer();
    for (var i = 0; i < s.length; i++) {
      if (i > 0 && (s.length - i) % 3 == 0) b.write(',');
      b.write(s[i]);
    }
    return b.toString();
  }
}

extension<E> on Iterable<E> {
  E? get firstOrNull => isEmpty ? null : first;
}
