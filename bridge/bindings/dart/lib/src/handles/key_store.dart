part of '../../blockchainer.dart';

/// A custodial store that seals signing secrets at rest. Create one with
/// [BlockchainerNodes.createKeyStore] or [BlockchainerNodes.loadKeyStore].
final class KeyStore {
  KeyStore._(this._bc, this._h);

  final Blockchainer _bc;
  final Pointer<Void> _h;

  DynamicLibrary get _lib => _bc.lib;

  /// Seals an account's [secret] and returns its 32 byte address.
  Uint8List addAccount(Uint8List publicKey, Uint8List secret) {
    final pa = calloc<Uint8>(32);
    try {
      _scoped([publicKey, secret], (p) {
        final rc = _lib.lookupFunction<_KsAddC, _KsAddD>(
                'blockchainer_keystore_add_account')(
            _h, p[0], publicKey.length, p[1], secret.length, pa);
        if (rc != 0) throw const BlockchainerException('keystore add failed');
      });
      return Uint8List.fromList(pa.asTypedList(32));
    } finally {
      calloc.free(pa);
    }
  }

  /// Decrypts the sealed secret for [address]. Requires unlocked.
  Uint8List reveal(Uint8List address) => _scoped(
        [address],
        (p) => _bc._take(
          _lib.lookupFunction<_BufNodeAddrC, _BufNodeAddrD>(
              'blockchainer_keystore_reveal')(_h, p[0]),
          'keystore_reveal',
        ),
      );

  /// The persistable encrypted blob.
  Uint8List serialise() => _bc._take(
        _lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(
            'blockchainer_keystore_serialise')(_h),
        'keystore_serialise',
      );

  /// Recovers the master key from [passphrase]. Throws when it is wrong.
  void unlock(String passphrase) => _withUtf8(passphrase, (cp) {
        final rc = _lib.lookupFunction<_KsUnlockC, _KsUnlockD>(
            'blockchainer_keystore_unlock')(_h, cp);
        if (rc != 0) {
          throw const BlockchainerException('keystore unlock failed');
        }
      });

  /// Whether secrets can currently be revealed.
  bool get unlocked =>
      _lib.lookupFunction<_IntOfHandleC, _IntOfHandleD>(
          'blockchainer_keystore_unlocked')(_h) ==
      1;

  /// Releases the native store.
  void dispose() => _lib.lookupFunction<_HandleFreeC, _HandleFreeD>(
      'blockchainer_keystore_free')(_h);
}
