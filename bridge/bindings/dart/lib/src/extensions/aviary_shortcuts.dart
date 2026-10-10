part of '../../aviary.dart';

/// Shortcuts for the common single validator setups.
extension AviaryQuick on Aviary {
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
      throw const AviaryException('chain assembly refused');
    }
    return chain;
  }
}

/// Handles released at the end of a scope, whatever [body] does.
extension AviaryScopes on Aviary {
  /// Runs [body] with a fresh single validator node and disposes it after.
  R withNode<R>(int chainId, R Function(Node node) body) {
    final node = createNode(chainId);
    try {
      return body(node);
    } finally {
      node.dispose();
    }
  }

  /// Runs [body] with an unlocked key store and releases it after.
  R withKeyStore<R>(String passphrase, R Function(KeyStore store) body) {
    final store = createKeyStore(passphrase);
    try {
      return body(store);
    } finally {
      store.dispose();
    }
  }

  /// Runs [body] with a built chain tree and frees it, hooks included, after.
  R withTree<R>(TreeNode root, R Function(ChainTree tree) body) {
    final tree = buildChainTree(root);
    try {
      return body(tree);
    } finally {
      tree.free();
    }
  }
}
