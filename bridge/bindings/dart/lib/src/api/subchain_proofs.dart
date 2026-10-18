part of '../../aviary.dart';

/// Subchain anchors and cross chain proofs.
/// {@category Subchains and trees}
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
        lib.fn<_ScAnchorC>('aviary_anchor_encode').asFunction<_ScAnchorD>()(
          childChainId,
          p[0],
          blob.length,
        ),
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
          lib
              .fn<_VerifyC>('aviary_subchain_proof_verify')
              .asFunction<_VerifyD>()(p[0], p[1], proof.length) ==
          1,
    );
  }
}

/// The parent side of a chain: anchoring children and proving into them.
/// {@category Subchains and trees}
extension ChainSubchains on Chain {
  /// Proposes the next block anchoring encoded anchors in ascending child
  /// id order. Throws on an invalid anchor.
  Uint8List proposeAnchored(int timestamp, List<Uint8List> anchors) {
    final blob = _blobList(anchors);
    return _scoped(
      [blob],
      (p) => _bc._take(
        _bc.lib
            .fn<_ScChainAnchoredC>('aviary_chain_propose_anchored')
            .asFunction<_ScChainAnchoredD>()(_h, timestamp, p[0], blob.length),
        'chain_propose_anchored',
      ),
    );
  }

  Uint8List _byChild(String fn, int childChainId) => _bc._take(
    _bc.lib.fn<_ScChainIdC>(fn).asFunction<_ScChainIdD>()(_h, childChainId),
    fn,
  );

  /// The last anchored header of a child.
  Uint8List childTip(int childChainId) =>
      _byChild('aviary_chain_child_tip', childChainId);

  /// A proof hop from this parent's current registry into a child.
  Uint8List subchainHop(int childChainId) =>
      _byChild('aviary_chain_subchain_hop', childChainId);

  /// A state inclusion proof for an address on this account model chain.
  Uint8List proveAccount(Uint8List address) => _scoped(
    [address],
    (p) => _bc._take(
      _bc.lib
          .fn<_ScChainAddrC>('aviary_chain_prove_account')
          .asFunction<_ScChainAddrD>()(_h, p[0]),
      'chain_prove_account',
    ),
  );
}
