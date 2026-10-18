part of '../../aviary.dart';

/// Subchains: registering children and binding them to a parent.
/// {@category Subchains and trees}
extension AviarySubchains on Aviary {
  /// The genesis header of a genesis, the identity a parent registers by.
  Uint8List genesisHeader(Uint8List genesis) =>
      _bufOp('aviary_genesis_header', genesis);

  /// Encodes a child registration. [maxDepth] is the child's own subtree
  /// depth, 0 for a leaf.
  Uint8List encodeChildRegistration({
    required int chainId,
    required Uint8List genesisHeader,
    required List<ValidatorEntry> validators,
    int maxDepth = 0,
  }) => _withU64s(
    _powers(validators),
    (pw) => _scoped(
      [genesisHeader, _publicKeys(validators)],
      (p) => _take(
        lib
            .fn<_ScChildRegC>('aviary_child_registration_encode')
            .asFunction<_ScChildRegD>()(
          chainId,
          p[0],
          genesisHeader.length,
          p[1],
          pw,
          validators.length,
          maxDepth,
        ),
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
        lib
            .fn<_ScPolicyC>('aviary_subchain_policy_encode')
            .asFunction<_ScPolicyD>()(
          maxChildren,
          maxDepth,
          maxAnchorsPerBlock,
          maxHeadersPerAnchor,
          p[0],
          blob.length,
        ),
        'subchain_policy_encode',
      ),
    );
  }

  /// Commits a policy into a genesis; the chain becomes a parent.
  Uint8List commitSubchainPolicy(Uint8List genesis, Uint8List policy) =>
      _buf2Op('aviary_genesis_commit_subchain_policy', genesis, policy);

  /// Binds a child genesis to its parent by id and 32 byte genesis hash.
  Uint8List commitParentBinding(
    Uint8List genesis,
    int parentChainId,
    Uint8List parentGenesisHash,
  ) => _scoped(
    [genesis, parentGenesisHash],
    (p) => _take(
      lib
          .fn<_ScBindingC>('aviary_genesis_commit_parent_binding')
          .asFunction<_ScBindingD>()(p[0], genesis.length, parentChainId, p[1]),
      'genesis_commit_parent_binding',
    ),
  );
}
