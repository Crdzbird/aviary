import 'dart:typed_data';

import 'package:aviary/aviary.dart';

/// Builds the record chains the dynamic subchain and persistence demos use:
/// BFT record chains with one validator each, and a dynamic subchain policy
/// whose single Ed25519 registrar signs registry operations.
final class RecordFamily {
  /// A family on [bc] whose registrar key is [registrarSecret], a 64 byte
  /// Ed25519 secret, with [parent] validating the parent chain and [child]
  /// the child chain.
  const factory RecordFamily(
    Aviary bc,
    Uint8List registrarSecret,
    KeyPairBytes parent,
    KeyPairBytes child,
  ) = RecordFamily._;

  const RecordFamily._(this.bc, this.registrarSecret, this.parent, this.child);

  final Aviary bc;
  final Uint8List registrarSecret;
  final KeyPairBytes parent;
  final KeyPairBytes child;

  /// A BFT record chain genesis for [chainId] validated by [validator].
  Uint8List genesis(int chainId, KeyPairBytes validator) => bc.commitBlueprint(
    bc.buildGenesis(
      chainId: chainId,
      accounts: [],
      validators: [(publicKey: validator.public, power: 1)],
      stateModel: 2,
    ),
    bc.makeBlueprint(consensus: 'bft', stateModel: 'record'),
  );

  /// The parent genesis with a dynamic policy: one registrar, threshold 1.
  Uint8List parentGenesis(int chainId) => bc.commitSubchainPolicy(
    genesis(chainId, parent),
    bc.encodeDynamicSubchainPolicy(
      threshold: 1,
      registrars: [
        (
          scheme: RegistrarScheme.ed25519,
          publicKey: registrarSecret.sublist(32),
        ),
      ],
    ),
  );

  /// A child genesis bound to the parent [parentId] whose hash is
  /// [parentHash].
  Uint8List childGenesis(int chainId, int parentId, Uint8List parentHash) =>
      bc.commitParentBinding(genesis(chainId, child), parentId, parentHash);

  /// Assembles [genesis] for [validator] with the block archive on.
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

  /// A registrar signed operation registering the child [chainId] whose
  /// genesis is [childGen] with the parent [parentId].
  Uint8List register(
    int chainId,
    Uint8List childGen,
    int parentId,
    Uint8List parentHash,
  ) => bc.signOp(
    bc.opRegister(
      bc.encodeChildRegistration(
        chainId: chainId,
        genesisHeader: bc.genesisHeader(childGen),
        validators: [(publicKey: child.public, power: 1)],
      ),
    ),
    parentChainId: parentId,
    parentGenesisHash: parentHash,
    registrarIndex: 0,
    secret: registrarSecret,
  );
}
