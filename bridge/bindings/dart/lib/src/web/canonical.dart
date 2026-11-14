part of '../aviary_web.dart';

/// Canonical bytes that are identical across every binding.
/// {@category Ledger}
extension AviaryCanonical on Aviary {
  /// The 32 byte BLAKE3 digest of [msg].
  Uint8List hash(Uint8List msg) => _engine.scoped(
    [msg],
    (p) => _engine.take('aviary_hash', [p[0], msg.length], 'hash'),
  );

  /// The canonical 32 byte transaction id of the given fields.
  Uint8List txid(
    int version,
    int chainId,
    Uint8List body,
    Uint8List author,
    Uint8List signature,
  ) => _engine.scoped(
    [body, author, signature],
    (p) => _engine.take('aviary_web_txid', [
      version,
      ..._halves(chainId, 'chainId'),
      p[0],
      body.length,
      p[1],
      author.length,
      p[2],
      signature.length,
    ], 'txid'),
  );

  /// The canonical 32 byte block header hash. The three roots are 32 bytes.
  Uint8List headerHash(
    int version,
    int chainId,
    int height,
    Uint8List parentHash,
    int timestamp,
    Uint8List txRoot,
    Uint8List stateRoot,
    Uint8List consensus,
  ) {
    _require32([parentHash, txRoot, stateRoot], 'headerHash');
    return _engine.scoped(
      [parentHash, txRoot, stateRoot, consensus],
      (p) => _engine.take('aviary_web_header_hash', [
        version,
        ..._halves(chainId, 'chainId'),
        ..._halves(height, 'height'),
        p[0],
        ..._halves(timestamp, 'timestamp'),
        p[1],
        p[2],
        p[3],
        consensus.length,
      ], 'headerHash'),
    );
  }
}
