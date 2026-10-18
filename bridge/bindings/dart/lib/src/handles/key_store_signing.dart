part of '../../aviary.dart';

/// Signing and inventory on a key store. The sealed secret is used inside
/// the engine and wiped; it never reaches Dart.
/// {@category Custody}
extension KeyStoreSigning on KeyStore {
  /// A signed, encoded transfer of [amount] from the stored account at
  /// [from] to [to]. The scheme follows the stored key (Ed25519 or hybrid);
  /// a zero [fee] gives the plain version 1 envelope. Throws when the store
  /// is locked or [from] is unknown.
  Uint8List signTransfer({
    required int chainId,
    required Uint8List from,
    required Uint8List to,
    required int amount,
    required int nonce,
    int fee = 0,
  }) => _scoped(
    [from, to],
    (p) => _bc._take(
      _lib.fn<_KsSignC>('aviary_keystore_sign_transfer').asFunction<_KsSignD>()(
        _h,
        chainId,
        p[0],
        p[1],
        amount,
        nonce,
        fee,
      ),
      'keystore_sign_transfer',
    ),
  );

  /// A signed, encoded record write by the stored account at [from]
  /// (record state model). The scheme follows the stored key, so a hybrid key
  /// signs with Ed25519 + ML-DSA-65. Throws when locked or [from] is unknown.
  Uint8List signRecord({
    required int chainId,
    required Uint8List from,
    required int nonce,
    required Uint8List key,
    required RecordOp op,
    required Uint8List value,
  }) => _scoped([from, key, value], (p) {
    final f = _lib
        .fn<_KsSignRecC>('aviary_keystore_sign_record')
        .asFunction<_KsSignRecD>();
    final r = f(_h, chainId, p[0], nonce, p[1], op.index, p[2], value.length);
    return _bc._take(r, 'keystore_sign_record');
  });

  /// The stored public key for [address]. Throws when unknown.
  Uint8List publicKey(Uint8List address) => _scoped(
    [address],
    (p) => _bc._take(
      _lib
          .fn<_BufNodeAddrC>('aviary_keystore_public_key')
          .asFunction<_BufNodeAddrD>()(_h, p[0]),
      'keystore_public_key',
    ),
  );

  /// Whether the store holds [address].
  bool has(Uint8List address) => _scoped(
    [address],
    (p) =>
        _lib.fn<_KsHasC>('aviary_keystore_has').asFunction<_KsHasD>()(
          _h,
          p[0],
        ) ==
        1,
  );

  /// Every stored address.
  List<Uint8List> addresses() => _splitBlobList(
    _bc._take(
      _lib
          .fn<_BufOfHandleC>('aviary_keystore_addresses')
          .asFunction<_BufOfHandleD>()(_h),
      'keystore_addresses',
    ),
  );

  /// Forgets the master key until the next [KeyStore.unlock].
  void lock() => _lib
      .fn<_HandleFreeC>('aviary_keystore_lock')
      .asFunction<_HandleFreeD>()(_h);
}
