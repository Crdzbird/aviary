import 'dart:convert';

import 'package:aviary_dart/aviary.dart';

import 'vector_scenario.dart';

/// Registration and anchoring: main blocks 1 and 2 of the scenario.
extension VectorScenarioSteps on VectorScenario {
  void register() {
    final registration = fx.registration(1001, userGenesis);
    put('registration', registration);
    final op = bc.opRegister(registration);
    put('op_register', op);
    put('op_register_digest', bc.opDigest(op, 1, mainHash));
    final signed = fx.sign(op, 1, mainHash);
    put('op_register_signed', signed);
    final header = mainBlock(10, ops: [signed]);
    put('header_1', header);
    put('header_1_hash', bc.encodedHeaderHash(header));
    put('subchain_root_1', bc.headerSubchainRoot(header));
    put('child_record_1', mainChain.childRecord(1001).encoded);
  }

  void anchor() {
    userChain.addTx(
      bc.signRecordTx(
        chainId: 1001,
        nonce: 0,
        key: recordKey,
        op: RecordOp.put,
        value: utf8.encode('door opened'),
        secret: fx.secret,
      ),
    );
    fx.commit(userChain, userChain.propose(20));
    fx.commit(userChain, userChain.propose(21));
    final anchor = bc.pendingAnchor(mainChain, userChain)!;
    put('anchor', anchor);
    final header = mainBlock(11, anchors: [anchor]);
    put('header_2_hash', bc.encodedHeaderHash(header));
    final root = bc.headerSubchainRoot(header);
    put('subchain_root_2', root);
    put('child_record_2', mainChain.childRecord(1001).encoded);
    final proof = mainChain.childProof(1001);
    put('child_proof_2', proof);
    put('child_proof_2_verified', bc.verifyChildProof(root, proof).encoded);
    put(
      'path_proof_2',
      bc.encodeSubchainProofV2([
        mainChain.subchainHopV2(1001),
      ], userChain.proveRecord(recordKey)),
    );
    put('main_snapshot_hash_2', bc.hash(mainChain.snapshot()));
    put('user_snapshot_hash_2', bc.hash(userChain.snapshot()));
  }
}
