part of '../../blockchainer.dart';

/// Signed transfers admitted to a node's mempool.
extension NodeTransfers on Node {
  /// Submits an Ed25519 transfer and returns its transaction id.
  Uint8List submitTransfer(
          Uint8List secret, Uint8List to, int amount, int nonce) =>
      _scoped(
        [secret, to],
        (p) => _bc._take(
          _lib.lookupFunction<_SubmitC, _SubmitD>(
                  'blockchainer_node_submit_transfer')(
              _h, p[0], secret.length, p[1], amount, nonce),
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
  ) =>
      _scoped(
        [secret, to],
        (p) => _bc._take(
          _lib.lookupFunction<_SubmitFeeC, _SubmitFeeD>(
                  'blockchainer_node_submit_transfer_fee')(
              _h, p[0], secret.length, p[1], amount, nonce, fee),
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
  ) =>
      _scoped(
        [secret, public, to],
        (p) => _bc._take(
          _lib.lookupFunction<_SubmitHybridC, _SubmitHybridD>(
                  'blockchainer_node_submit_transfer_hybrid')(_h, p[0],
              secret.length, p[1], public.length, p[2], amount, nonce),
          'submit_transfer_hybrid',
        ),
      );
}
