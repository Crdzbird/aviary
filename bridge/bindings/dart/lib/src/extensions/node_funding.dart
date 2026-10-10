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
