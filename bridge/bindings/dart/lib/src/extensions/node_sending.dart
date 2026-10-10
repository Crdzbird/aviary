part of '../../aviary.dart';

/// Transfers that pick the nonce and the signature scheme for you.
extension NodeSending on Node {
  /// Sends [amount] from [from] to the address [to]. The nonce is read from
  /// the chain, so submit one transfer per account per block this way; use
  /// [NodeTransfers.submitTransfer] with explicit nonces for more. The
  /// signature scheme follows the account; hybrid accounts carry no [fee].
  /// Returns the transaction id.
  Uint8List send(Account from, Uint8List to, int amount, {int fee = 0}) {
    final next = nonce(from.address);
    if (from.isHybrid) {
      if (fee != 0) {
        throw ArgumentError.value(fee, 'fee', 'hybrid transfers carry no fee');
      }
      return submitTransferHybrid(
          from.secret, from.publicKey, to, amount, next);
    }
    if (fee != 0) return submitTransferFee(from.secret, to, amount, next, fee);
    return submitTransfer(from.secret, to, amount, next);
  }

  /// [send] to another account.
  Uint8List sendTo(Account from, Account to, int amount, {int fee = 0}) =>
      send(from, to.address, amount, fee: fee);

  /// Produces the next block and returns its height. [timestamp] defaults to
  /// the new height, a monotone placeholder for callers that do not track
  /// time.
  int commit({int? timestamp}) => produceBlock(timestamp ?? height() + 1);
}
