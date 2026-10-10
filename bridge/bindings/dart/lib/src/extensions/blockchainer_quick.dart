part of '../../blockchainer.dart';

/// Shortcuts for the common single validator setups.
extension BlockchainerQuick on Blockchainer {
  /// A BLS validator key pair from 32 bytes of fresh random key material.
  KeyPairBytes newValidator() {
    final rng = Random.secure();
    final material = Uint8List.fromList(
      List<int>.generate(32, (_) => rng.nextInt(256)),
    );
    return newBLSValidator(material);
  }

  /// Builds a genesis for [validator] alone, commits [blueprint] (bft over
  /// the account model when null) and assembles the chain for it. Throws
  /// when the registry or posture refuses the mixture.
  Chain assembleSingleValidator({
    required int chainId,
    required KeyPairBytes validator,
    List<GenesisAccountEntry> accounts = const [],
    Blueprint? blueprint,
    int assuranceLevel = 1,
    int stateModel = 0,
    int timestamp = 1,
  }) {
    final base = buildGenesis(
      chainId: chainId,
      accounts: accounts,
      validators: [validator.entry(1)],
      timestamp: timestamp,
      assuranceLevel: assuranceLevel,
      stateModel: stateModel,
    );
    final mixture =
        blueprint ?? makeBlueprint(consensus: 'bft', stateModel: 'account');
    final genesis = commitBlueprint(base, mixture);
    final chain = assembleChain(genesis, 0, validator.public, validator.secret);
    if (chain == null) {
      throw const BlockchainerException('chain assembly refused');
    }
    return chain;
  }
}
