part of '../../aviary.dart';

/// Subchain anchors and cross chain proofs.
extension AviarySubchainProofs on Aviary {
  /// The encoded header of an encoded block.
  Uint8List blockHeader(Uint8List block) =>
      _bufOp('aviary_block_header', block);

  /// A header's 32 byte subchain root, zero below version 3.
  Uint8List headerSubchainRoot(Uint8List header) =>
      _bufOp('aviary_header_subchain_root', header);

  /// A child header paired with the certificate that finalized it.
  Uint8List certifiedHeader(Uint8List header, Uint8List certificate) =>
      _buf2Op('aviary_certified_header_encode', header, certificate);

  /// An anchor: one child's consecutive certified headers.
  Uint8List encodeAnchor(int childChainId, List<Uint8List> certifiedHeaders) {
    final blob = _blobList(certifiedHeaders);
    return _scoped(
      [blob],
      (p) => _take(
        lib.lookupFunction<_ScAnchorC, _ScAnchorD>('aviary_anchor_encode')(
            childChainId, p[0], blob.length),
        'anchor_encode',
      ),
    );
  }

  /// A cross chain proof from hops down the tree plus a state proof.
  Uint8List encodeSubchainProof(List<Uint8List> hops, Uint8List stateProof) =>
      _buf2Op('aviary_subchain_proof_encode', _blobList(hops), stateProof);

  /// Whether [proof] is committed under a parent's 32 byte subchain root all
  /// the way down and its state proof holds.
  bool verifySubchainProof(Uint8List parentSubchainRoot, Uint8List proof) {
    if (parentSubchainRoot.length != 32) return false;
    return _scoped(
      [parentSubchainRoot, proof],
      (p) =>
          lib.lookupFunction<_VerifyC, _VerifyD>(
              'aviary_subchain_proof_verify')(p[0], p[1], proof.length) ==
          1,
    );
  }
}
