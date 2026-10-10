part of '../../blockchainer.dart';

/// Subchains: registering children and binding them to a parent.
extension BlockchainerSubchains on Blockchainer {
  /// The genesis header of a genesis, the identity a parent registers by.
  Uint8List genesisHeader(Uint8List genesis) =>
      _bufOp('blockchainer_genesis_header', genesis);

  /// Encodes a child registration. [maxDepth] is the child's own subtree
  /// depth, 0 for a leaf.
  Uint8List encodeChildRegistration({
    required int chainId,
    required Uint8List genesisHeader,
    required List<ValidatorEntry> validators,
    int maxDepth = 0,
  }) =>
      _withU64s(
        _powers(validators),
        (pw) => _scoped(
          [genesisHeader, _publicKeys(validators)],
          (p) => _take(
            lib.lookupFunction<_ScChildRegC, _ScChildRegD>(
                    'blockchainer_child_registration_encode')(chainId, p[0],
                genesisHeader.length, p[1], pw, validators.length, maxDepth),
            'child_registration_encode',
          ),
        ),
      );

  /// Encodes a parent's subchain policy from encoded registrations, which
  /// must be ascending and unique by chain id. Zero bounds are unlimited.
  Uint8List encodeSubchainPolicy({
    required int maxChildren,
    required List<Uint8List> children,
    int maxDepth = 1,
    int maxAnchorsPerBlock = 0,
    int maxHeadersPerAnchor = 0,
  }) {
    final blob = _blobList(children);
    return _scoped(
      [blob],
      (p) => _take(
        lib.lookupFunction<_ScPolicyC, _ScPolicyD>(
                'blockchainer_subchain_policy_encode')(maxChildren, maxDepth,
            maxAnchorsPerBlock, maxHeadersPerAnchor, p[0], blob.length),
        'subchain_policy_encode',
      ),
    );
  }

  /// Commits a policy into a genesis; the chain becomes a parent.
  Uint8List commitSubchainPolicy(Uint8List genesis, Uint8List policy) =>
      _buf2Op('blockchainer_genesis_commit_subchain_policy', genesis, policy);

  /// Binds a child genesis to its parent by id and 32 byte genesis hash.
  Uint8List commitParentBinding(
    Uint8List genesis,
    int parentChainId,
    Uint8List parentGenesisHash,
  ) =>
      _scoped(
        [genesis, parentGenesisHash],
        (p) => _take(
          lib.lookupFunction<_ScBindingC, _ScBindingD>(
                  'blockchainer_genesis_commit_parent_binding')(
              p[0], genesis.length, parentChainId, p[1]),
          'genesis_commit_parent_binding',
        ),
      );
}
