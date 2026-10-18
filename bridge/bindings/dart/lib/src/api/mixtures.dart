part of '../../aviary.dart';

/// Composable chains: mixtures, registry discovery and assembly.
/// {@category Chains}
extension AviaryMixtures on Aviary {
  /// A validator BLS key pair from [keyMaterial].
  KeyPairBytes newBLSValidator(Uint8List keyMaterial) => _scoped(
    [keyMaterial],
    (p) => _splitKeyBlob(
      _take(
        lib.fn<_BufOfBytesC>('aviary_bls_keygen').asFunction<_BufOfBytesD>()(
          p[0],
          keyMaterial.length,
        ),
        'bls_keygen',
      ),
    ),
  );

  /// Declares a mixture of engine, state model and proposer ids plus params.
  Blueprint makeBlueprint({
    required String consensus,
    required String stateModel,
    String proposer = '',
    Uint8List? params,
  }) {
    final pr = params ?? Uint8List(0);
    return _withUtf8(
      consensus,
      (cc) => _withUtf8(
        stateModel,
        (cs) => _withUtf8(
          proposer,
          (cp) => _scoped(
            [pr],
            (p) => Blueprint._(
              this,
              _take(
                lib
                    .fn<_BpEncodeC>('aviary_blueprint_encode')
                    .asFunction<_BpEncodeD>()(cc, cs, cp, p[0], pr.length),
                'blueprint_encode',
              ),
            ),
          ),
        ),
      ),
    );
  }

  List<String> _registryIds(int kind) {
    final b = _take(
      lib.fn<_RegistryIdsC>('aviary_registry_ids').asFunction<_RegistryIdsD>()(
        kind,
      ),
      'registry_ids',
    );
    return [for (final id in _splitBlobList(b)) String.fromCharCodes(id)];
  }

  /// The consensus engines a mixture may name.
  List<String> engineIds() => _registryIds(0);

  /// The state models a mixture may name.
  List<String> stateModelIds() => _registryIds(1);

  /// The proposer policies a mixture may name.
  List<String> proposerIds() => _registryIds(2);

  /// Assembles a node for validator [me]. Null means a fail closed reject.
  Chain? assembleChain(
    Uint8List genesis,
    int me,
    Uint8List blsPub,
    Uint8List blsSec,
  ) => _scoped([genesis, blsPub, blsSec], (p) {
    final h =
        lib
            .fn<_ChainAssembleC>('aviary_chain_assemble')
            .asFunction<_ChainAssembleD>()(
          p[0],
          genesis.length,
          me,
          p[1],
          blsPub.length,
          p[2],
          blsSec.length,
        );
    if (h == nullptr) return null;
    return Chain._(this, h);
  });
}
