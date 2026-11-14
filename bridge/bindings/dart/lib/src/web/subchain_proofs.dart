part of '../aviary_web.dart';

/// Subchain anchors and cross chain proofs. In a browser: reading headers
/// out of blocks.
/// {@category Subchains and trees}
extension AviarySubchainProofs on Aviary {
  /// The encoded header of an encoded block.
  Uint8List blockHeader(Uint8List block) => _engine.scoped(
    [block],
    (p) => _engine.take('aviary_block_header', [
      p[0],
      block.length,
    ], 'aviary_block_header'),
  );

  /// A header's 32 byte subchain root, zero below version 3.
  Uint8List headerSubchainRoot(Uint8List header) => _engine.scoped(
    [header],
    (p) => _engine.take('aviary_header_subchain_root', [
      p[0],
      header.length,
    ], 'aviary_header_subchain_root'),
  );
}
