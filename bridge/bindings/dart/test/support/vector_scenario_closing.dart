import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'vector_scenario.dart';

/// Closing, and the wider registry: main block 3 and the wide chain.
extension VectorScenarioClosing on VectorScenario {
  void close() {
    final record = mainChain.childRecord(1001);
    final op = bc.opClose(
      chainId: 1001,
      finalHeight: record.tipHeight,
      finalTipHash: record.tipHash,
    );
    put('op_close', op);
    put('op_close_digest', bc.opDigest(op, 1, mainHash));
    final header = mainBlock(12, ops: [fx.sign(op, 1, mainHash)]);
    put('header_3_hash', bc.encodedHeaderHash(header));
    put('subchain_root_3', bc.headerSubchainRoot(header));
    put('child_record_3', mainChain.childRecord(1001).encoded);
    put('main_snapshot_hash_3', bc.hash(mainChain.snapshot()));
  }

  void wide() {
    final genesis = fx.parentGenesis(2);
    final hash = bc.genesisHash(genesis);
    final chain = fx.assemble(genesis, fx.server);
    final ops = <Uint8List>[
      for (var id = 2000; id < 2011; id++)
        fx.sign(
          bc.opRegister(fx.registration(id, fx.recordGenesis(id, fx.user))),
          2,
          hash,
        ),
    ];
    final block = fx.commit(chain, chain.proposeSubchain(10, ops: ops));
    final root = bc.headerSubchainRoot(bc.blockHeader(block));
    put('wide_root', root);
    final proof = chain.childProof(2007);
    put('wide_proof', proof);
    put('wide_proof_verified', bc.verifyChildProof(root, proof).encoded);
    chain.free();
  }
}
