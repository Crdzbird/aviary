import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';
import 'dynamic_fixture.dart';

/// The scenario behind vectors/subchain_v2_vectors.txt, replayed through the
/// Dart binding. Each step records the values it produces under the names
/// the vector file uses.
final class VectorScenario {
  VectorScenario(this.bc, Uint8List secret) : fx = DynamicFixture(bc, secret) {
    put('registrar_secret', secret);
    put('policy', fx.policy);
    mainGenesis = fx.parentGenesis(1);
    mainHash = bc.genesisHash(mainGenesis);
    put('main_genesis_hash', mainHash);
    put('main_genesis_header', bc.genesisHeader(mainGenesis));
    userGenesis = fx.childGenesis(1001, 1, mainHash);
    mainChain = fx.assemble(mainGenesis, fx.server);
    userChain = fx.assemble(userGenesis, fx.user);
  }

  final Aviary bc;
  final DynamicFixture fx;
  final Map<String, String> produced = {};
  final Uint8List recordKey = fill(0x42);
  late final Uint8List mainGenesis;
  late final Uint8List mainHash;
  late final Uint8List userGenesis;
  late final Chain mainChain;
  late final Chain userChain;

  void put(String name, Uint8List value) => produced[name] = value.hex;

  /// The header of the block [ops] and [anchors] produce on the main chain.
  Uint8List mainBlock(
    int timestamp, {
    List<Uint8List> anchors = const [],
    List<Uint8List> ops = const [],
  }) => bc.blockHeader(
    fx.commit(
      mainChain,
      mainChain.proposeSubchain(timestamp, anchors: anchors, ops: ops),
    ),
  );

  void free() {
    mainChain.free();
    userChain.free();
  }
}
