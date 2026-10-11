part of '../../aviary.dart';

/// A running single validator chain node.
/// {@category Ledger}
final class Node extends _NativeHandle {
  Node._(Aviary bc, Pointer<Void> h) : super(bc, h, 'aviary_node_free');

  DynamicLibrary get _lib => _bc.lib;

  Uint8List _bufOfNode(String fn, String op) =>
      _bc._take(_lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(fn)(_h), op);

  /// A fresh Ed25519 account.
  Account newAccount() {
    final secret = _bufOfNode('aviary_node_new_account', 'new_account');
    final pub = Uint8List.sublistView(secret, 32, 64);
    return Account(secret, pub, accountAddress(pub));
  }

  /// A fresh hybrid post quantum account; spend it with
  /// [NodeTransfers.submitTransferHybrid].
  Account newAccountHybrid() {
    final kp = _splitKeyBlob(
      _bufOfNode('aviary_node_new_account_hybrid', 'new_account_hybrid'),
    );
    return Account(kp.secret, kp.public, accountAddress(kp.public));
  }

  /// The 32 byte chain address of a public key.
  Uint8List accountAddress(Uint8List publicKey) => _scoped(
    [publicKey],
    (p) => _bc._take(
      _lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('aviary_account_address')(
        p[0],
        publicKey.length,
      ),
      'account_address',
    ),
  );

  /// Credits an address at genesis. Throws once a block has been produced.
  void credit(Uint8List address, int amount) => _scoped([address], (p) {
    final rc = _lib.lookupFunction<_CreditC, _CreditD>('aviary_node_credit')(
      _h,
      p[0],
      amount,
    );
    if (rc == 0) return;
    final status = AviaryStatus.fromCode(rc);
    final why = status == AviaryStatus.genesisSealed
        ? 'genesis is sealed; fund accounts with transfers'
        : 'credit failed (status $rc)';
    throw AviaryException(why, status);
  });

  /// Produces the next block and returns its height.
  int produceBlock(int timestamp) {
    final out = calloc<Uint64>();
    try {
      final rc = _lib.lookupFunction<_ProduceC, _ProduceD>(
        'aviary_node_produce_block',
      )(_h, timestamp, out);
      if (rc != 0) throw const AviaryException('produce_block failed');
      return out.value;
    } finally {
      calloc.free(out);
    }
  }

  /// Releases the native node.
  void dispose() {
    _nodeSinks.remove(_h.address)?.close();
    _release();
  }

  /// Alias of [dispose].
  void free() => dispose();
}
