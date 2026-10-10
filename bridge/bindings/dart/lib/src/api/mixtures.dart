part of '../../aviary.dart';

/// A declared mixture of engine, state model and proposer ids plus params.
/// Commit it with [AviaryGenesis.commitBlueprint].
final class Blueprint {
  Blueprint._(this._bc, this.encoded);

  final Aviary _bc;

  /// The canonical bytes.
  final Uint8List encoded;

  /// The 32 byte commitment to the mixture.
  Uint8List hash() => _scoped(
        [encoded],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
              'aviary_blueprint_hash')(p[0], encoded.length),
          'blueprint_hash',
        ),
      );
}

/// Composable chains: mixtures, registry discovery and assembly.
extension AviaryMixtures on Aviary {
  /// A validator BLS key pair from [keyMaterial].
  KeyPairBytes newBLSValidator(Uint8List keyMaterial) => _scoped(
        [keyMaterial],
        (p) => _splitKeyBlob(_take(
          lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('aviary_bls_keygen')(
              p[0], keyMaterial.length),
          'bls_keygen',
        )),
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
                lib.lookupFunction<_BpEncodeC, _BpEncodeD>(
                    'aviary_blueprint_encode')(cc, cs, cp, p[0], pr.length),
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
      lib.lookupFunction<_RegistryIdsC, _RegistryIdsD>('aviary_registry_ids')(
          kind),
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
          Uint8List genesis, int me, Uint8List blsPub, Uint8List blsSec) =>
      _scoped([genesis, blsPub, blsSec], (p) {
        final h = lib.lookupFunction<_ChainAssembleC, _ChainAssembleD>(
                'aviary_chain_assemble')(
            p[0], genesis.length, me, p[1], blsPub.length, p[2], blsSec.length);
        if (h == nullptr) return null;
        return Chain._(this, h);
      });
}
