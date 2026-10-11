part of '../../aviary.dart';

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
        _bc.lib.lookupFunction<_ScChainAnchoredC, _ScChainAnchoredD>(
          'aviary_chain_propose_anchored',
        )(_h, timestamp, p[0], blob.length),
        'chain_propose_anchored',
      ),
    );
  }

  Uint8List _byChild(String fn, int childChainId) => _bc._take(
    _bc.lib.lookupFunction<_ScChainIdC, _ScChainIdD>(fn)(_h, childChainId),
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
      _bc.lib.lookupFunction<_ScChainAddrC, _ScChainAddrD>(
        'aviary_chain_prove_account',
      )(_h, p[0]),
      'chain_prove_account',
    ),
  );
}
