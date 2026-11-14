part of '../aviary_native.dart';

/// The parent side of a chain with a dynamic policy: registry operations,
/// records and child proofs.
/// {@category Subchains and trees}
extension ChainRegistry on Chain {
  /// Proposes the next block carrying encoded [anchors] in ascending child
  /// id order and signed registry [ops], applied operations first. Throws
  /// with the reason when an anchor or an operation is invalid.
  Uint8List proposeSubchain(
    int timestamp, {
    List<Uint8List> anchors = const [],
    List<Uint8List> ops = const [],
  }) {
    final a = anchors.isEmpty ? Uint8List(0) : _blobList(anchors);
    final o = ops.isEmpty ? Uint8List(0) : _blobList(ops);
    return _scoped(
      [a, o],
      (p) => _bc._take(
        _bc.lib
            .fn<_ProposeSubchainC>('aviary_chain_propose_subchain')
            .asFunction<_ProposeSubchainD>()(
          _h,
          timestamp,
          p[0],
          a.length,
          p[1],
          o.length,
        ),
        'chain_propose_subchain',
      ),
    );
  }

  /// The registry's sizes and authority epoch, or null when this chain has
  /// no dynamic policy.
  RegistryCounts? registryCounts() {
    final out = calloc<Uint32>(3);
    try {
      final rc = _bc.lib
          .fn<_RegistryCountsC>('aviary_chain_registry_counts')
          .asFunction<_RegistryCountsD>()(_h, out, out + 1, out + 2);
      if (rc != 1) return null;
      return (records: out[0], open: out[1], authorityEpoch: out[2]);
    } finally {
      calloc.free(out);
    }
  }

  /// The current record of a child. Throws when this chain has no dynamic
  /// policy or the child was never registered.
  ChildRecord childRecord(int childChainId) =>
      childRecordOf(_byChild('aviary_chain_child_record', childChainId));

  /// A proof that a child's record is committed by this parent's current
  /// subchain root.
  Uint8List childProof(int childChainId) =>
      _byChild('aviary_chain_child_proof', childChainId);

  /// A proof hop from this parent's current registry into a child, tagged
  /// with its kind in the first byte: 1 for a version 1 policy, 2 for a
  /// dynamic policy.
  Uint8List subchainHopV2(int childChainId) =>
      _byChild('aviary_chain_subchain_hop_v2', childChainId);
}
