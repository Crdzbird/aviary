part of '../../aviary.dart';

/// Signed transfers admitted to a node's mempool.
extension NodeTransfers on Node {
  /// Submits an Ed25519 transfer and returns its transaction id.
  Uint8List submitTransfer(
    Uint8List secret,
    Uint8List to,
    int amount,
    int nonce,
  ) => _scoped(
    [secret, to],
    (p) => _bc._take(
      _lib.lookupFunction<_SubmitC, _SubmitD>('aviary_node_submit_transfer')(
        _h,
        p[0],
        secret.length,
        p[1],
        amount,
        nonce,
      ),
      'submit_transfer',
    ),
  );

  /// Submits a transfer carrying a priority [fee].
  Uint8List submitTransferFee(
    Uint8List secret,
    Uint8List to,
    int amount,
    int nonce,
    int fee,
  ) => _scoped(
    [secret, to],
    (p) => _bc._take(
      _lib.lookupFunction<_SubmitFeeC, _SubmitFeeD>(
        'aviary_node_submit_transfer_fee',
      )(_h, p[0], secret.length, p[1], amount, nonce, fee),
      'submit_transfer_fee',
    ),
  );

  /// Submits a hybrid post quantum transfer with the blobs from
  /// [Node.newAccountHybrid].
  Uint8List submitTransferHybrid(
    Uint8List secret,
    Uint8List public,
    Uint8List to,
    int amount,
    int nonce,
  ) => _scoped(
    [secret, public, to],
    (p) => _bc._take(
      _lib.lookupFunction<_SubmitHybridC, _SubmitHybridD>(
        'aviary_node_submit_transfer_hybrid',
      )(_h, p[0], secret.length, p[1], public.length, p[2], amount, nonce),
      'submit_transfer_hybrid',
    ),
  );

  /// Admits an already signed, encoded transaction, for example from
  /// [KeyStoreSigning.signTransfer]. Returns its transaction id.
  Uint8List submitTx(Uint8List encodedTx) => _scoped(
    [encodedTx],
    (p) => _bc._take(
      _lib.lookupFunction<_ChainBlockC, _ChainBlockD>('aviary_node_submit_tx')(
        _h,
        p[0],
        encodedTx.length,
      ),
      'submit_tx',
    ),
  );

  /// Signs a transfer from the [store] account at [from] and admits it. The
  /// secret never leaves the engine. Returns the transaction id.
  Uint8List submitTransferFromStore(
    KeyStore store,
    Uint8List from,
    Uint8List to,
    int amount,
    int nonce, {
    int fee = 0,
  }) => _scoped(
    [from, to],
    (p) => _bc._take(
      _lib.lookupFunction<_SubmitKsC, _SubmitKsD>(
        'aviary_node_submit_transfer_keystore',
      )(_h, store._h, p[0], p[1], amount, nonce, fee),
      'submit_transfer_keystore',
    ),
  );
}
