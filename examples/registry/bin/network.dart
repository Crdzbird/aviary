import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

const chainId = 32;
const sites = ['plant', 'lab', 'warehouse', 'office'];
final bc = Aviary.instance;

/// The registry run by four sites instead of one process: each site is a
/// validator, an auditor follows as an observer, and every message between
/// them goes through a mesh the app owns. Writes reach consensus through
/// `setTransactions`; every node then answers record queries and proofs.
void main() {
  final keys = List.generate(sites.length, (_) => bc.newValidator());
  final genesis = bc.buildGenesis(
    chainId: chainId,
    accounts: const [],
    validators: [for (final k in keys) k.entry(1)],
    stateModel: 2,
  );
  final net = PeerMesh([
    for (var i = 0; i < sites.length; i++)
      bc.newNetNode(genesis: genesis, me: i, blsKey: keys[i]),
    bc.newNetNode(genesis: genesis, me: AviaryNetwork.observer),
  ])..linkAll();
  final auditor = net.nodes.last;
  final facilities = bc.createNode(1).newAccount();
  final lab = bc.createNode(2).newAccount();
  final pump = bc.hash(utf8.encode('asset:PUMP-0042'));
  final scope = bc.hash(utf8.encode('asset:MICRO-0007'));

  print('== Height 1: facilities registers two machines');
  final n = net.nodes[0].recordNonce(facilities.address);
  propose(net, [
    write(facilities, n, pump, RecordOp.put, doc('in service', 'B')),
    write(facilities, n + 1, scope, RecordOp.put, doc('in storage', 'A')),
  ], start: true);
  showEverywhere(net, pump, 'pump');

  print('\n== Height 2: the microscope moves to the lab');
  final t = net.nodes[1].recordNonce(facilities.address);
  propose(net, [write(facilities, t, scope, RecordOp.transfer, lab.address)]);

  print('\n== Height 3: the lab updates the machine it now owns');
  final l = net.nodes[2].recordNonce(lab.address);
  propose(net, [write(lab, l, scope, RecordOp.put, doc('in use', 'A3'))]);
  showEverywhere(net, scope, 'microscope');

  print('\n== The auditor checks a site\'s proof against its own state root');
  final proof = net.nodes[3].proveRecord(scope);
  final ok = bc.verifyStateProof(auditor.stateRoot(), proof);
  final leaf = bc.recordFromProof(proof);
  print(
    '  proof from ${sites[3]}: ${ok ? 'verified' : 'rejected'}, '
    'version ${leaf.version}, owner ${leaf.owner.shortHex}',
  );
  final forged = Uint8List.fromList(proof)..[8] ^= 1;
  final bad = bc.verifyStateProof(auditor.stateRoot(), forged);
  print('  tampered proof: ${bad ? 'verified' : 'rejected'}');
  print('  unknown key: ${auditor.record(bc.hash(utf8.encode('none')))}');
  net.free();
}

/// Hands [txs] to every validator, then starts or advances the height so the
/// proposer includes them, and checks every node committed the same height.
void propose(PeerMesh net, List<Uint8List> txs, {bool start = false}) {
  for (final node in net.nodes.take(sites.length)) {
    node.setTransactions(txs);
  }
  start ? net.start() : net.advance();
  if (!net.settle(validators: List.generate(sites.length, (i) => i))) {
    throw StateError('the sites did not commit');
  }
  final height = net.nodes.map((n) => n.height).toSet().single;
  print('  height $height committed on all ${net.nodes.length} nodes');
}

Uint8List write(Account a, int n, Uint8List k, RecordOp op, Uint8List v) =>
    bc.signRecordTx(
      chainId: chainId,
      nonce: n,
      key: k,
      op: op,
      value: v,
      secret: a.secret,
    );

Uint8List doc(String status, String building) =>
    utf8.encode(jsonEncode({'status': status, 'building': building}));

/// Prints what each site and the auditor hold for [key].
void showEverywhere(PeerMesh net, Uint8List key, String label) {
  for (var i = 0; i < net.nodes.length; i++) {
    final rec = net.nodes[i].record(key)!;
    final who = i < sites.length ? sites[i] : 'auditor';
    final body = utf8.decode(rec.value);
    print('  $who: $label v${rec.version} owner ${rec.owner.shortHex} $body');
  }
}
