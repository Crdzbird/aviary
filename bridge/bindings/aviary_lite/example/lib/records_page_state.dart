import 'dart:convert';
import 'dart:typed_data';

import 'package:flutter/material.dart';
import 'package:aviary_lite/aviary_lite.dart';

import 'record_text.dart';
import 'records_page.dart';

/// Owns a single validator record chain and two signing accounts.
final class RecordsPageState extends State<RecordsPage> {
  final _bc = Aviary.instance;
  late final _chain = _bc.assembleRecordChain(
    chainId: 31,
    validator: _bc.newValidator(),
  );
  late final _people = _bc.createNode(1);
  late final Account _facilities = _people.newAccount();
  late final Account _lab = _people.newAccount();
  late final _key = _bc.hash(utf8.encode('asset:PUMP-0042'));
  var _owner = 0;
  var _visits = 0;
  var _status = 'No record yet.';

  Account get _current => _owner == 0 ? _facilities : _lab;

  bool _write(RecordOp op, List<int> value, String done) {
    try {
      _chain.write(_current.secret, _key, op, Uint8List.fromList(value));
      _chain.produceBlock(_chain.height + 2);
      setState(() => _status = '$done\n${_describe()}');
      return true;
    } on AviaryException catch (e) {
      setState(() => _status = 'Refused: ${e.message}');
      return false;
    }
  }

  String _describe() =>
      describeRecord(_chain.record(_key), _facilities.address);

  void _register() => _write(
    RecordOp.put,
    utf8.encode('{"status":"in service"}'),
    'Registered.',
  );

  void _service() => _write(
    RecordOp.put,
    utf8.encode('{"status":"serviced","visits":${++_visits}}'),
    'Serviced.',
  );

  void _handOver() {
    final moved = _write(RecordOp.transfer, _lab.address, 'Handed over.');
    _owner = moved ? 1 : _owner;
  }

  void _verify() {
    final proof = _chain.proveRecord(_key);
    final ok = _bc.verifyStateProof(_chain.stateRoot(), proof);
    final leaf = _bc.recordFromProof(proof);
    setState(
      () => _status =
          'Proof against the state root: $ok, version ${leaf.version}',
    );
  }

  @override
  void dispose() {
    _chain.free();
    _people.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.all(16),
    child: Column(
      children: [
        Text(_status, textAlign: TextAlign.center),
        const SizedBox(height: 16),
        Wrap(
          spacing: 8,
          runSpacing: 8,
          children: [
            FilledButton(onPressed: _register, child: const Text('Register')),
            FilledButton(onPressed: _service, child: const Text('Service')),
            OutlinedButton(
              onPressed: _handOver,
              child: const Text('Hand to lab'),
            ),
            OutlinedButton(onPressed: _verify, child: const Text('Verify')),
          ],
        ),
      ],
    ),
  );
}
