part of '../aviary_web.dart';

/// Dynamic subchain registration. In a browser: the header hash a child
/// record holds.
/// {@category Subchains and trees}
extension AviaryDynamicSubchains on Aviary {
  /// The canonical 32 byte hash of an encoded header of any version: what a
  /// record's tip hash and genesis header hash hold.
  Uint8List encodedHeaderHash(Uint8List header) => _engine.scoped(
    [header],
    (p) => _engine.take('aviary_encoded_header_hash', [
      p[0],
      header.length,
    ], 'aviary_encoded_header_hash'),
  );
}
