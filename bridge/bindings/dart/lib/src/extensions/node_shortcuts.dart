part of '../../aviary.dart';

/// Account creation with genesis funding in one step.
extension NodeFunding on Node {
  /// A fresh Ed25519 account credited with [amount] at genesis. Throws once
  /// the first block has been produced.
  Account fundAccount(int amount) {
    final account = newAccount();
    credit(account.address, amount);
    return account;
  }

  /// A fresh hybrid post quantum account credited with [amount] at genesis.
  Account fundHybridAccount(int amount) {
    final account = newAccountHybrid();
    credit(account.address, amount);
    return account;
  }
}

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

  /// Sends [amount] from the key store account at [from] with the nonce read
  /// from the chain; the secret never leaves the engine.
  Uint8List sendFromStore(
    KeyStore store,
    Uint8List from,
    Uint8List to,
    int amount, {
    int fee = 0,
  }) =>
      submitTransferFromStore(store, from, to, amount, nonce(from), fee: fee);

  /// [send] to another account.
  Uint8List sendTo(Account from, Account to, int amount, {int fee = 0}) =>
      send(from, to.address, amount, fee: fee);

  /// Produces the next block and returns its height. [timestamp] defaults to
  /// the new height, a monotone placeholder for callers that do not track
  /// time.
  int commit({int? timestamp}) => produceBlock(timestamp ?? height() + 1);
}

/// Decoded views of a node's state.
extension NodeViews on Node {
  /// Whether [proof] from [NodeQueries.proveAccount] holds under the
  /// current state root.
  bool verifyAccountProof(Uint8List proof) =>
      _bc.verifyStateProof(stateRoot(), proof);

  /// Prometheus metric values keyed by sample name with its labels verbatim.
  Map<String, double> metricValues() {
    final out = <String, double>{};
    for (final line in metrics().split('\n')) {
      if (line.isEmpty || line.startsWith('#')) continue;
      final space = line.lastIndexOf(' ');
      if (space < 0) continue;
      final value = double.tryParse(line.substring(space + 1));
      if (value != null) out[line.substring(0, space)] = value;
    }
    return out;
  }
}
