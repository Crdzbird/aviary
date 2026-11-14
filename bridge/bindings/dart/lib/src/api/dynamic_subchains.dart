part of '../aviary_native.dart';

/// Subchains version 2: a parent with a dynamic policy registers children in
/// any block and commits its registry as a Merkle tree.
/// {@category Subchains and trees}
extension AviaryDynamicSubchains on Aviary {
  /// Encodes one registrar.
  Uint8List encodeRegistrar(RegistrarEntry registrar) => _scoped(
    [registrar.publicKey],
    (p) => _take(
      lib
          .fn<_DynRegistrarC>('aviary_registrar_encode')
          .asFunction<_DynRegistrarD>()(
        registrar.scheme.code,
        p[0],
        registrar.publicKey.length,
      ),
      'registrar_encode',
    ),
  );

  Uint8List _registrarList(List<RegistrarEntry> registrars) {
    if (registrars.isEmpty) return Uint8List(0);
    return _blobList([for (final r in registrars) encodeRegistrar(r)]);
  }

  /// Encodes a dynamic policy; commit it with
  /// [AviarySubchains.commitSubchainPolicy]. [maxChildren] bounds open
  /// children and zero bounds are unlimited. No [registrars] with a
  /// [threshold] of 0 is a registry sealed at genesis. [children] are encoded
  /// registrations made at genesis, ascending and unique by chain id.
  Uint8List encodeDynamicSubchainPolicy({
    int maxDepth = 1,
    int maxChildren = 0,
    int maxAnchorsPerBlock = 0,
    int maxHeadersPerAnchor = 0,
    int maxOpsPerBlock = 0,
    int threshold = 0,
    List<RegistrarEntry> registrars = const [],
    List<Uint8List> children = const [],
  }) {
    final keys = _registrarList(registrars);
    final kids = children.isEmpty ? Uint8List(0) : _blobList(children);
    return _scoped(
      [keys, kids],
      (p) => _take(
        lib
            .fn<_DynPolicyC>('aviary_dynamic_subchain_policy_encode')
            .asFunction<_DynPolicyD>()(
          maxDepth,
          maxChildren,
          maxAnchorsPerBlock,
          maxHeadersPerAnchor,
          maxOpsPerBlock,
          threshold,
          p[0],
          keys.length,
          p[1],
          kids.length,
        ),
        'dynamic_subchain_policy_encode',
      ),
    );
  }

  /// The canonical 32 byte hash of an encoded header of any version: what a
  /// record's tip hash and genesis header hash hold.
  Uint8List encodedHeaderHash(Uint8List header) =>
      _bufOp('aviary_encoded_header_hash', header);
}
