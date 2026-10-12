import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

/// An equipment registry: every machine is a record whose value is a JSON
/// document, owned by the department that registered it. Writes are signed
/// and finalized in BFT blocks, ownership moves with a transfer, and anyone
/// holding the chain's state root can verify a record without trusting the
/// registry.
void main() {
  final bc = Aviary.instance;
  final chain = bc.assembleRecordChain(
    chainId: 31,
    validator: bc.newValidator(),
  );
  final facilities = bc.createNode(1).newAccount();
  final lab = bc.createNode(2).newAccount();
  final auditor = Auditor(bc);

  final pump = bc.hash(utf8.encode('asset:PUMP-0042'));
  final microscope = bc.hash(utf8.encode('asset:MICRO-0007'));

  section('Register two machines (facilities signs both)');
  chain.write(
    facilities.secret,
    pump,
    RecordOp.put,
    doc({
      'model': 'Grundfos CR 15',
      'location': 'Building B, plant room',
      'status': 'in service',
      'serviceLog': [],
    }),
  );
  chain.write(
    facilities.secret,
    microscope,
    RecordOp.put,
    doc({
      'model': 'Zeiss Axio',
      'location': 'Building A, storage',
      'status': 'in storage',
      'serviceLog': [],
    }),
  );
  print('  queued ${chain.pendingCount} signed writes');
  chain.produceBlock(1);
  show(chain, pump, 'pump');
  show(chain, microscope, 'microscope');

  section('Service visit: facilities updates the pump (version grows)');
  final current = decode(chain.record(pump)!.value);
  current['serviceLog'] = [
    ...current['serviceLog'] as List,
    {'date': '2026-10-12', 'work': 'replaced seal', 'by': 'J. Osei'},
  ];
  chain.write(facilities.secret, pump, RecordOp.put, doc(current));
  chain.produceBlock(2);
  show(chain, pump, 'pump');

  section('The lab tries to edit the microscope before it owns it');
  try {
    chain.write(
      lab.secret,
      microscope,
      RecordOp.put,
      doc({'status': 'in use'}),
    );
  } on AviaryException catch (e) {
    print('  refused: ${e.message}');
  }

  section('Facilities transfers the microscope to the lab, the lab updates it');
  chain.write(facilities.secret, microscope, RecordOp.transfer, lab.address);
  chain.produceBlock(3);
  chain.write(
    lab.secret,
    microscope,
    RecordOp.put,
    doc({
      'model': 'Zeiss Axio',
      'location': 'Building A, lab 3',
      'status': 'in use',
      'serviceLog': [],
    }),
  );
  chain.produceBlock(4);
  show(chain, microscope, 'microscope');
  try {
    chain.write(
      facilities.secret,
      microscope,
      RecordOp.put,
      doc({'status': 'x'}),
    );
  } on AviaryException catch (e) {
    print('  former owner refused: ${e.message}');
  }

  section('An auditor verifies the pump record from the state root alone');
  auditor.trust(chain.stateRoot());
  final verdict = auditor.verify(chain.proveRecord(pump));
  print('  ${verdict ?? 'proof rejected'}');
  final tampered = Uint8List.fromList(chain.proveRecord(pump))..[8] ^= 1;
  print('  tampered proof: ${auditor.verify(tampered) ?? 'rejected'}');

  section('Decommission: the pump is deleted, its key is free again');
  chain.write(facilities.secret, pump, RecordOp.delete, Uint8List(0));
  chain.produceBlock(5);
  print('  pump record: ${chain.record(pump)}');

  section('What consensus saw');
  for (final e in chain.events().whereType<BlockFinalized>()) {
    print('  block ${e.height}: ${e.txCount} record transactions');
  }
  print(
    '  ${chain.metricValues()['aviary_blocks_finalized_total']!.toInt()} blocks finalized',
  );
  chain.free();
}

/// Someone who never ran the chain: they hold a state root they trust (for
/// example from a light client) and check records against it.
final class Auditor {
  Auditor(this.bc);

  final Aviary bc;
  Uint8List? _root;

  void trust(Uint8List stateRoot) => _root = stateRoot;

  String? verify(Uint8List proof) {
    final root = _root;
    if (root == null || !bc.verifyStateProof(root, proof)) return null;
    final leaf = bc.recordFromProof(proof);
    final body = decode(leaf.value);
    return 'verified version ${leaf.version} of ${body['model']} at '
        '${body['location']}, owner ${leaf.owner.shortHex}';
  }
}

Uint8List doc(Map<String, Object?> m) => utf8.encode(jsonEncode(m));

Map<String, Object?> decode(Uint8List bytes) =>
    jsonDecode(utf8.decode(bytes)) as Map<String, Object?>;

void section(String title) => print('\n== $title');

void show(Chain chain, Uint8List key, String label) {
  final rec = chain.record(key)!;
  final body = decode(rec.value);
  print(
    '  $label v${rec.version} owner ${rec.owner.shortHex}: '
    '${body['status']} at ${body['location']}',
  );
}
