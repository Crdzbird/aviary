part of '../aviary_native.dart';

/// Saving a chain and keeping its blocks.
/// {@category Chains}
extension ChainPersistence on Chain {
  /// The current tip header, the genesis header before any block.
  Uint8List tip() => _buf('aviary_chain_tip');

  /// Everything this chain needs to resume after a restart: the ledger, the
  /// receipts of the tip block, the engine's consensus state and, when
  /// enabled, the block archive. It holds no secret key and is bound to the
  /// genesis hash. Transactions queued for the next proposal are not saved.
  Uint8List snapshot() => _buf('aviary_chain_snapshot');

  /// Starts keeping every block this chain commits from now on, with its
  /// commit certificate, inside the handle and its snapshots. Throws when
  /// the engine is not BFT.
  void enableArchive() {
    final rc = _bc.lib
        .fn<_IntOfHandleC>('aviary_chain_archive_enable')
        .asFunction<_IntOfHandleD>()(_h);
    if (rc != 1) throw const AviaryException('the archive needs a BFT engine');
  }

  /// The archived height range, or null while the archive is empty.
  ArchiveRange? archiveRange() {
    final out = calloc<Uint64>(2);
    try {
      final rc = _bc.lib
          .fn<_ArchiveRangeC>('aviary_chain_archive_range')
          .asFunction<_ArchiveRangeD>()(_h, out, out + 1);
      if (rc != 1) return null;
      return (first: out[0], last: out[1]);
    } finally {
      calloc.free(out);
    }
  }

  /// The archived encoded block at [height].
  Uint8List archivedBlock(int height) =>
      _byChild('aviary_chain_archived_block', height);

  /// The commit certificate of the archived block at [height].
  Uint8List archivedCertificate(int height) =>
      _byChild('aviary_chain_archived_certificate', height);

  /// Drops archived blocks below [height] and returns how many.
  int pruneArchive(int height) => _bc.lib
      .fn<_TreeU64IdC>('aviary_chain_archive_prune')
      .asFunction<_TreeU64IdD>()(_h, height);
}
