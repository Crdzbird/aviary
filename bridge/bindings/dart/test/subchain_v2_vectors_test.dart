import 'package:aviary_dart/aviary.dart';
import 'package:test/test.dart';

import 'support/check.dart';
import 'support/subchain_vectors.dart';
import 'support/vector_scenario.dart';
import 'support/vector_scenario_closing.dart';
import 'support/vector_scenario_steps.dart';

void main() {
  test('subchain v2 golden vectors', () {
    final pinned = loadSubchainVectors();
    if (pinned == null) {
      markTestSkipped('vectors/subchain_v2_vectors.txt is not available');
      return;
    }
    final bc = Aviary.instance;
    final scenario = VectorScenario(bc, pinned['registrar_secret']!.hexBytes);
    scenario.register();
    scenario.anchor();
    scenario.close();
    scenario.wide();
    for (final entry in scenario.produced.entries) {
      check(pinned[entry.key] == entry.value, 'vector ${entry.key}');
    }
    check(
      scenario.produced.length == pinned.length,
      'every pinned vector is replayed',
    );
    final closed = bc.decodeChildRecord(pinned['child_record_3']!.hexBytes);
    check(
      closed.status == ChildStatus.closed &&
          closed.closedHeight == 3 &&
          closed.tipHeight == 2 &&
          closed.chainId == 1001,
      'the pinned closed record decodes',
    );
    final wide = bc.verifyChildProof(
      pinned['wide_root']!.hexBytes,
      pinned['wide_proof']!.hexBytes,
    );
    check(wide.chainId == 2007 && wide.index == 7, 'pinned wide proof holds');
    check(
      bc.verifySubchainProofV2(
            pinned['subchain_root_2']!.hexBytes,
            pinned['path_proof_2']!.hexBytes,
          ) &&
          !bc.verifySubchainProofV2(
            pinned['subchain_root_1']!.hexBytes,
            pinned['path_proof_2']!.hexBytes,
          ),
      'pinned path proof holds under its own root only',
    );
    scenario.free();
  });
}
