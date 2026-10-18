part of '../../aviary.dart';

/// Signed transfers admitted to a node's mempool.
/// {@category Ledger}
extension NodeTransfers on Node {
  /// Submits an Ed25519 transfer and returns its transaction id.
  Uint8List submitTransfer(
    Uint8List secret,
    Uint8List to,
    int amount,
    int nonce,
  ) {
    final f = _lib.fn<_SubmitC>('aviary_node_submit_transfer');
    final g = f.asFunction<_SubmitD>();
    return _scoped([secret, to], (p) {
      final r = g(_h, p[0], secret.length, p[1], amount, nonce);
      return _bc._take(r, 'submit_transfer');
    });
  }

  /// Submits a transfer carrying a priority [fee].
  Uint8List submitTransferFee(
    Uint8List secret,
    Uint8List to,
    int amount,
    int nonce,
    int fee,
  ) {
    final f = _lib.fn<_SubmitFeeC>('aviary_node_submit_transfer_fee');
    final g = f.asFunction<_SubmitFeeD>();
    return _scoped([secret, to], (p) {
      final r = g(_h, p[0], secret.length, p[1], amount, nonce, fee);
      return _bc._take(r, 'submit_transfer_fee');
    });
  }

  /// Submits a hybrid post quantum transfer with the blobs from
  /// [Node.newAccountHybrid].
  Uint8List submitTransferHybrid(
    Uint8List secret,
    Uint8List public,
    Uint8List to,
    int amount,
    int nonce,
  ) {
    final f = _lib.fn<_SubmitHybridC>('aviary_node_submit_transfer_hybrid');
    final g = f.asFunction<_SubmitHybridD>();
    final s = secret.length, k = public.length;
    return _scoped([secret, public, to], (p) {
      final r = g(_h, p[0], s, p[1], k, p[2], amount, nonce);
      return _bc._take(r, 'submit_transfer_hybrid');
    });
  }

  /// Admits an already signed, encoded transaction, for example from
  /// [KeyStoreSigning.signTransfer]. Returns its transaction id.
  Uint8List submitTx(Uint8List encodedTx) {
    final f = _lib.fn<_ChainBlockC>('aviary_node_submit_tx');
    final g = f.asFunction<_ChainBlockD>();
    return _scoped([encodedTx], (p) {
      return _bc._take(g(_h, p[0], encodedTx.length), 'submit_tx');
    });
  }

  /// Signs a transfer from the [store] account at [from] and admits it. The
  /// secret never leaves the engine. Returns the transaction id.
  Uint8List submitTransferFromStore(
    KeyStore store,
    Uint8List from,
    Uint8List to,
    int amount,
    int nonce, {
    int fee = 0,
  }) {
    final f = _lib.fn<_SubmitKsC>('aviary_node_submit_transfer_keystore');
    final g = f.asFunction<_SubmitKsD>();
    return _scoped([from, to], (p) {
      final r = g(_h, store._h, p[0], p[1], amount, nonce, fee);
      return _bc._take(r, 'submit_transfer_keystore');
    });
  }
}
