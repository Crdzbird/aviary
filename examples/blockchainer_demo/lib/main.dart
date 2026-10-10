// Blockchainer demo — a Flutter dashboard driving the entire stack through the
// C ABI: create a chain, fund accounts, send transactions, produce blocks, watch
// a live event stream, read observability metrics, verify a light-client proof,
// and derive multi-chain interop addresses.
import 'dart:async';
import 'dart:convert';
import 'dart:typed_data';

import 'package:blockchainer/blockchainer.dart';
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

bool _eqBytes(Uint8List a, Uint8List b) {
  if (a.length != b.length) return false;
  for (var i = 0; i < a.length; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

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
  final List<String> _membership = [];
  final List<String> _mixture = [];
  final List<String> _subchain = [];
  final List<String> _tree = [];

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
          _membershipCard(),
          _mixtureCard(),
          _subchainCard(),
          _treeCard(),
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

  // Anonymous membership (CredentialGate Option B): a member proves it holds an
  // issuer-signed credential without revealing its identity, carrying a
  // per-(epoch, scope) pseudonym — the same within an epoch (Sybil-resistant),
  // unlinkable across epochs. The nym seed is blind-committed, so the issuer
  // never learns it.
  void _runMembershipDemo() {
    try {
      final bc = _bc!;
      final issuer = bc.newBBSIssuer(Uint8List(32)..fillRange(0, 32, 0x2a));
      final policy = bc.makeCredentialPolicy(
        chainId: 42,
        issuerPublic: issuer.public,
        header: Uint8List.fromList('blockchainer.credential.v2'.codeUnits),
        minTier: 2,
        mode: 1, // pseudonymous (Option B)
      );
      final memberSecret = Uint8List(32)..fillRange(0, 32, 0x77);
      final mc = policy.membershipCommit(memberSecret);
      final bi = policy.blindIssue(issuer.secret, extractCommitment(mc), 3, 4102444800, 1);
      final cred = bc.finalizeMembership(mc, bi);

      final gate = bc.makeCredentialGate(policy);
      Uint8List ch(int b) => Uint8List(32)..fillRange(0, 32, b);
      const now = 1700000000;
      Uint8List? admit(int epoch, String scope, int c) => gate.admitPseudonymous(
          epoch, scope, ch(c), policy.presentPseudonymous(cred, epoch, scope, ch(c)), now);

      final log = <String>[];
      final nymA = admit(5, 'gossip', 1);
      log.add('admitted · pseudonym ${_short(nymA!)}…');
      final nym2 = admit(5, 'gossip', 2); // fresh challenge, same epoch+scope
      log.add(_eqBytes(nym2!, nymA)
          ? 'epoch 5 again → SAME pseudonym (Sybil-linkable ✓)'
          : 'epoch 5 again → different (unexpected)');
      log.add(gate.noteAdmission(5, 'gossip', nymA) && !gate.noteAdmission(5, 'gossip', nym2)
          ? 'Sybil cap: 2nd session on the nym refused ✓'
          : 'Sybil cap unexpected');
      final nymE = admit(6, 'gossip', 1); // next epoch
      log.add(!_eqBytes(nymE!, nymA)
          ? 'epoch 6 → DIFFERENT pseudonym (unlinkable ✓)'
          : 'epoch 6 → same (unexpected)');
      gate.revokeNym(nymA);
      final banned = admit(5, 'gossip', 3) == null;
      log.add(banned ? 'revoked nym → refused in epoch 5 ✓' : 'revocation unexpected');
      gate.free();
      setState(() => _membership
        ..clear()
        ..addAll(log));
    } catch (e) {
      setState(() => _membership
        ..clear()
        ..add('error: $e'));
    }
  }

  Widget _membershipCard() => _section('Anonymous membership (BBS pseudonyms)', Icons.badge, [
        const Text(
            'A permissioned chain admits a member by a zero-knowledge proof of an '
            'issuer-signed credential — no identity revealed — carrying a '
            'per-epoch pseudonym for Sybil resistance and targeted revocation.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        FilledButton.icon(
            onPressed: _runMembershipDemo,
            icon: const Icon(Icons.how_to_reg),
            label: const Text('Run membership demo')),
        if (_membership.isNotEmpty) ...[
          const SizedBox(height: 10),
          ..._membership.map((l) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Text(l, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)))),
        ],
      ]);

  // Composable chains: a chain is a MIXTURE of pluggable components (engine,
  // state model, proposer policy) named by stable ids. The mixture is committed
  // into the genesis hash — it IS chain identity — and an unknown id or a mix the
  // assurance posture forbids is rejected at assembly (fail-closed).
  void _runMixtureDemo() {
    try {
      final bc = _bc!;
      final log = <String>[];
      log.add('engines: ${bc.engineIds().join(", ")}');
      log.add('state models: ${bc.stateModelIds().join(", ")} · proposers: ${bc.proposerIds().join(", ")}');

      final v = bc.newBLSValidator(Uint8List(32)..fillRange(0, 32, 0x60));
      final accounts = [(address: Uint8List(32)..fillRange(0, 32, 0x11), balance: 1000)];
      final validators = [(publicKey: v.public, power: 1)];
      final genesis = bc.buildGenesis(chainId: 42, accounts: accounts, validators: validators);

      // Mix 1: PoS + account + round-robin. Committing it changes chain identity.
      final pos = bc.makeBlueprint(consensus: 'pos', stateModel: 'account', proposer: 'round-robin');
      final mixed = bc.commitBlueprint(genesis, pos);
      log.add('genesis ${_short(bc.genesisHash(genesis))}… → with mixture ${_short(bc.genesisHash(mixed))}… (identity changed ✓)');
      final chain = bc.assembleChain(mixed, 0, v.public, v.secret)!;
      final blk = chain.propose(2);
      final ok = chain.submit(blk, chain.seal(blk));
      log.add('pos/account/round-robin → propose → seal → submit: ${ok ? "height ${chain.height} ✓" : "failed"}');
      chain.free();

      // Mix 2: PoW with a difficulty target in params.
      final pow = bc.makeBlueprint(consensus: 'pow', stateModel: 'account', params: Uint8List(32)..fillRange(0, 32, 0xff));
      final pc = bc.assembleChain(bc.commitBlueprint(genesis, pow), 0, v.public, v.secret)!;
      final cand = pc.propose(2);
      final okPow = pc.submit(pc.mine(cand));
      log.add('pow/account → propose → mine → submit: ${okPow ? "height ${pc.height} ✓" : "failed"}');
      pc.free();

      // Fail-closed: a mixture nobody registered, and one the posture forbids.
      final unknown = bc.assembleChain(bc.commitBlueprint(genesis, bc.makeBlueprint(consensus: 'my-custom-engine', stateModel: 'account')), 0, v.public, v.secret);
      log.add(unknown == null ? 'unregistered "my-custom-engine" → rejected ✓' : 'unexpected');
      final hardened = bc.buildGenesis(chainId: 42, assuranceLevel: 2, accounts: accounts, validators: validators);
      final forbidden = bc.assembleChain(bc.commitBlueprint(hardened, pos), 0, v.public, v.secret);
      log.add(forbidden == null ? 'hardened posture + predictable-leader pos → rejected ✓' : 'unexpected');
      setState(() => _mixture..clear()..addAll(log));
    } catch (e) {
      setState(() => _mixture..clear()..add('error: $e'));
    }
  }

  void _runSubchainDemo() {
    try {
      final bc = _bc!;
      final log = <String>[];
      final cv = bc.newBLSValidator(Uint8List(32)..fillRange(0, 32, 0x71));
      final pv = bc.newBLSValidator(Uint8List(32)..fillRange(0, 32, 0x72));
      final alice = Uint8List(32)..fillRange(0, 32, 0xA1);
      final bft = bc.makeBlueprint(consensus: 'bft', stateModel: 'account');

      // The child: chain 42, BFT (finality = a commit certificate), alice funded.
      final childGen = bc.commitBlueprint(
          bc.buildGenesis(chainId: 42, accounts: [(address: alice, balance: 1000)], validators: [(publicKey: cv.public, power: 1)]), bft);
      final childHeader = bc.genesisHeader(childGen);
      // The parent: chain 1, registering the child by its genesis header + validator set.
      var parentGen = bc.commitBlueprint(bc.buildGenesis(chainId: 1, accounts: [], validators: [(publicKey: pv.public, power: 1)]), bft);
      final reg = bc.encodeChildRegistration(chainId: 42, genesisHeader: childHeader, validators: [(publicKey: cv.public, power: 1)]);
      final policy = bc.encodeSubchainPolicy(maxChildren: 1, maxHeadersPerAnchor: 8, children: [reg]);
      final plain = bc.genesisHash(parentGen);
      parentGen = bc.commitSubchainPolicy(parentGen, policy);
      log.add('parent genesis ${_short(plain)}… → with subchain policy ${_short(bc.genesisHash(parentGen))}… (identity changed ✓)');
      // The child binds to its parent: its hash changes, its genesis header does not.
      final bound = bc.commitParentBinding(childGen, 1, bc.genesisHash(parentGen));
      log.add('child bound to parent: hash ${_short(bc.genesisHash(childGen))}… → ${_short(bc.genesisHash(bound))}… (cannot be re-parented ✓)');

      final child = bc.assembleChain(bound, 0, cv.public, cv.secret)!;
      final parent = bc.assembleChain(parentGen, 0, pv.public, pv.secret)!;
      // The child finalizes two blocks with real certificates.
      final certified = <Uint8List>[];
      for (var ts = 2; ts <= 3; ts++) {
        final blk = child.propose(ts);
        final cert = child.certify(blk);
        child.submit(blk, cert);
        certified.add(bc.certifiedHeader(bc.blockHeader(blk), cert));
      }
      log.add('child (BFT) finalized ${child.height} blocks with commit certificates');
      // One parent block anchors the whole segment; every parent validator re-verifies it.
      final anchor = bc.encodeAnchor(42, certified);
      final pblk = parent.proposeAnchored(2, [anchor]);
      final okParent = parent.submit(pblk, parent.certify(pblk));
      log.add('parent block anchors child[1..2]: ${okParent ? "height ${parent.height}, child tip recorded ✓" : "failed"}');
      // Tamper: a flipped certificate byte, and a stale re-anchor — both refused before proposal.
      final tampered = Uint8List.fromList(certified[1])..[certified[1].length - 1] ^= 1;
      var refused = false;
      try { parent.proposeAnchored(3, [bc.encodeAnchor(42, [tampered])]); } catch (_) { refused = true; }
      log.add(refused ? 'tampered child certificate → anchor refused ✓' : 'unexpected');
      refused = false;
      try { parent.proposeAnchored(3, [anchor]); } catch (_) { refused = true; }
      log.add(refused ? 're-anchoring an already-anchored height → refused ✓' : 'unexpected');
      // Cross-chain proof: alice's balance on the CHILD, verified from the PARENT header alone.
      final root = bc.headerSubchainRoot(bc.blockHeader(pblk));
      final proof = bc.encodeSubchainProof([parent.subchainHop(42)], child.proveAccount(alice));
      final ok = bc.verifySubchainProof(root, proof);
      final bad = Uint8List.fromList(proof)..[proof.length - 1] ^= 1;
      final badOk = bc.verifySubchainProof(root, bad);
      log.add('alice on chain 42 proven from chain 1 header (subchain_root ${_short(root)}…): ${ok ? "verified ✓" : "FAILED"}; tampered proof: ${badOk ? "accepted ✗" : "rejected ✓"}');
      child.free();
      parent.free();
      setState(() => _subchain..clear()..addAll(log));
    } catch (e) {
      setState(() => _subchain..clear()..add('error: $e'));
    }
  }

  void _runTreeDemo() {
    try {
      final bc = _bc!;
      final log = <String>[];
      ({Uint8List publicKey, Uint8List? secret, int power}) val(int seed) {
        final v = bc.newBLSValidator(Uint8List(32)..fillRange(0, 32, seed));
        return (publicKey: v.public, secret: v.secret, power: 1);
      }
      final alice = Uint8List(32)..fillRange(0, 32, 0xA1);
      // One declaration for the whole family: a PoS root, a 3-validator BFT
      // child, a funded BFT grandchild, and a sibling leaf.
      final root = TreeNode(chainId: 1, blueprint: bc.makeBlueprint(consensus: 'pos', stateModel: 'account', proposer: 'round-robin'),
          validators: [val(0x01)], children: [
        TreeNode(chainId: 42, validators: [val(0x10), val(0x11), val(0x12)], maxHeadersPerAnchor: 4, children: [
          TreeNode(chainId: 420, validators: [val(0x20), val(0x21)], accounts: [(address: alice, balance: 1000)]),
        ]),
        TreeNode(chainId: 43, validators: [val(0x30)]),
      ]);
      final tree = bc.buildChainTree(root);
      log.add('built ${tree.chainIds().length} chains in one go: ${tree.chainIds().join(" → ")} (genesis bottom-up, bindings top-down)');
      final finalized = <int, int>{};
      tree.onFinalized = (id, _) => finalized[id] = (finalized[id] ?? 0) + 1;
      tree.produce(420, 2); tree.produce(420, 3);
      log.add('grandchild 420 finalized 2 blocks → child 42 has ${tree.pendingAnchors(42)} pending anchor');
      tree.produce(42, 2); tree.produce(43, 2); tree.produce(1, 2);
      log.add('produced on 42, 43 and the root → heights root ${tree.height(1)}, 42: ${tree.height(42)}, 420: ${tree.height(420)}; anchors flowed upward ✓');
      log.add('finalization hook (Dart) saw: ${finalized.entries.map((e) => "${e.key}×${e.value}").join(", ")}');
      final proof = tree.proveAccount(420, alice);
      final ok = tree.verify(proof);
      final bad = Uint8List.fromList(proof)..[proof.length - 1] ^= 1;
      log.add("alice on grandchild 420 proven from the ROOT header: ${ok ? "verified ✓" : "FAILED"}; tampered: ${tree.verify(bad) ? "accepted ✗" : "rejected ✓"}");
      var refused = false;
      try {
        bc.buildChainTree(root.copyWith(children: [root.children[0].copyWith(blueprint: root.blueprint), root.children[1]]));
      } catch (_) {
        refused = true;
      }
      log.add(refused ? 'a PoS child in the spec → refused (only BFT children can be anchored) ✓' : 'unexpected');
      tree.free();
      setState(() => _tree..clear()..addAll(log));
    } catch (e) {
      setState(() => _tree..clear()..add('error: $e'));
    }
  }

  Widget _treeCard() => _section('Chain tree (declare a family of chains)', Icons.park, [
        const Text(
            'Declare a root chain, its subchains and their subchains — each with its '
            'own mixture, accounts and validators — and build them all at once. '
            'Producing on any chain anchors its children upward; anything on any '
            'descendant is provable from the root header. Hooks let Dart supply '
            'transactions and observe finalization.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        FilledButton.icon(onPressed: _runTreeDemo, icon: const Icon(Icons.account_tree), label: const Text('Run chain-tree demo')),
        if (_tree.isNotEmpty) ...[
          const SizedBox(height: 10),
          ..._tree.map((l) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Text(l, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)))),
        ],
      ]);

  Widget _subchainCard() => _section('Subchains (a block anchors a blockchain)', Icons.account_tree_outlined, [
        const Text(
            'A parent block carries a child chain\'s finalized headers + certificates; '
            'parent validators verify them with the light-client rule and commit the '
            'child\'s tip in the parent header. Anything on the child can then be proven '
            'from the parent\'s header alone.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        FilledButton.icon(onPressed: _runSubchainDemo, icon: const Icon(Icons.layers), label: const Text('Run subchain demo')),
        if (_subchain.isNotEmpty) ...[
          const SizedBox(height: 10),
          ..._subchain.map((l) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Text(l, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)))),
        ],
      ]);

  Widget _mixtureCard() => _section('Composable chains (mixtures)', Icons.tune, [
        const Text(
            'A chain is a mixture of pluggable components — consensus engine, state '
            'model, proposer policy — named by id. The mixture is committed into the '
            'genesis hash, and an unknown or posture-forbidden mix is refused.',
            style: TextStyle(color: Colors.white70, fontSize: 13)),
        const SizedBox(height: 10),
        FilledButton.icon(onPressed: _runMixtureDemo, icon: const Icon(Icons.science), label: const Text('Run mixture demo')),
        if (_mixture.isNotEmpty) ...[
          const SizedBox(height: 10),
          ..._mixture.map((l) => Padding(
              padding: const EdgeInsets.symmetric(vertical: 2),
              child: Text(l, style: const TextStyle(fontFamily: 'monospace', fontSize: 12)))),
        ],
      ]);

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
