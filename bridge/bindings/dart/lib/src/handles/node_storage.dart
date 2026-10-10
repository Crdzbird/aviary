part of '../../aviary.dart';

/// Durable state: the whole chain as one blob, and files around it.
extension NodeStorage on Node {
  /// The chain id, the genesis credits and every finalized block with its
  /// certificate as one blob. Pending mempool transactions are not included.
  Uint8List snapshot() => _bufOfNode('aviary_node_snapshot', 'snapshot');

  /// The encoded finalized block at [height], 1 based. Throws when absent.
  Uint8List blockAt(int height) => _bc._take(
        _lib.lookupFunction<_ChainProposeC, _ChainProposeD>(
            'aviary_node_block_at')(_h, height),
        'block_at',
      );

  /// Writes [snapshot] to [path] atomically: a sibling temporary file is
  /// written and flushed, then renamed over [path].
  Future<void> saveTo(String path) async {
    final bytes = snapshot();
    final tmp = File('$path.tmp');
    await tmp.writeAsBytes(bytes, flush: true);
    await tmp.rename(path);
  }
}

/// Rebuilding nodes from durable state.
extension AviaryNodeStorage on Aviary {
  /// A node rebuilt from [snapshot]. Every block is replayed through
  /// consensus, so each certificate is verified; a malformed blob throws
  /// with [AviaryStatus.badInput].
  Node restoreNode(Uint8List snapshot) => _scoped([snapshot], (p) {
        final h = lib.lookupFunction<_KsLoadC, _KsLoadD>('aviary_node_restore')(
            p[0], snapshot.length);
        if (h == nullptr) {
          throw const AviaryException(
              'node restore refused', AviaryStatus.badInput);
        }
        return Node._(this, h);
      });

  /// [restoreNode] from the file at [path], or null when there is no file.
  Future<Node?> restoreNodeFrom(String path) async {
    final file = File(path);
    if (!await file.exists()) return null;
    return restoreNode(await file.readAsBytes());
  }
}
