import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

import 'bytes.dart';

/// Builders shared by the dynamic subchain suites: record chains with one
/// BFT validator, a dynamic policy with one Ed25519 registrar ([secret] is
/// its 64 byte key), and signed registry operations.
final class DynamicFixture {
  DynamicFixture(
    this.bc,
    this.secret, {
    int serverSeed = 0x01,
    int userSeed = 0x10,
  }) : server = bc.newBLSValidator(fill(serverSeed)),
       user = bc.newBLSValidator(fill(userSeed));

  final Aviary bc;
  final Uint8List secret;
  final KeyPairBytes server;
  final KeyPairBytes user;

  RegistrarEntry get registrar =>
      (scheme: SignatureScheme.ed25519, publicKey: secret.sublist(32));

  Uint8List get policy =>
      bc.encodeDynamicSubchainPolicy(threshold: 1, registrars: [registrar]);

  Uint8List recordGenesis(int chainId, KeyPairBytes validator) =>
      bc.commitBlueprint(
        bc.buildGenesis(
          chainId: chainId,
          accounts: [],
          validators: [(publicKey: validator.public, power: 1)],
          stateModel: 2,
        ),
        bc.makeBlueprint(consensus: 'bft', stateModel: 'record'),
      );

  /// A parent genesis for [chainId] with [policy] committed.
  Uint8List parentGenesis(int chainId) =>
      bc.commitSubchainPolicy(recordGenesis(chainId, server), policy);

  /// A user chain genesis bound to the parent [parentId].
  Uint8List childGenesis(int chainId, int parentId, Uint8List parentHash) => bc
      .commitParentBinding(recordGenesis(chainId, user), parentId, parentHash);

  /// Assembles [genesis] for [validator] with the block archive enabled.
  Chain assemble(Uint8List genesis, KeyPairBytes validator) {
    final chain = bc.assembleChain(
      genesis,
      0,
      validator.public,
      validator.secret,
    );
    if (chain == null) throw StateError('assembly refused');
    chain.enableArchive();
    return chain;
  }

  /// Certifies and applies [block] on [chain], returning the block.
  Uint8List commit(Chain chain, Uint8List block) {
    if (!chain.submit(block, chain.certify(block))) {
      throw StateError('block refused');
    }
    return block;
  }

  /// The registration of the chain whose genesis is [genesis].
  Uint8List registration(int chainId, Uint8List genesis) =>
      bc.encodeChildRegistration(
        chainId: chainId,
        genesisHeader: bc.genesisHeader(genesis),
        validators: [(publicKey: user.public, power: 1)],
      );

  /// [op] approved by the registrar for the parent [parentId].
  Uint8List sign(Uint8List op, int parentId, Uint8List parentHash) => bc.signOp(
    op,
    parentChainId: parentId,
    parentGenesisHash: parentHash,
    registrarIndex: 0,
    secret: secret,
  );
}
