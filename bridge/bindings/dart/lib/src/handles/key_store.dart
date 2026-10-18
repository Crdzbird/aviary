part of '../../aviary.dart';

/// A custodial store that seals signing secrets at rest. Create one with
/// [AviaryNodes.createKeyStore] or [AviaryNodes.loadKeyStore].
/// {@category Custody}
final class KeyStore extends _NativeHandle {
  KeyStore._(Aviary bc, Pointer<Void> h) : super(bc, h, 'aviary_keystore_free');

  DynamicLibrary get _lib => _bc.lib;

  /// Seals an account's [secret] and returns its 32 byte address.
  Uint8List addAccount(Uint8List publicKey, Uint8List secret) {
    final pa = calloc<Uint8>(32);
    try {
      _scoped([publicKey, secret], (p) {
        final rc =
            _lib
                .fn<_KsAddC>('aviary_keystore_add_account')
                .asFunction<_KsAddD>()(
              _h,
              p[0],
              publicKey.length,
              p[1],
              secret.length,
              pa,
            );
        if (rc != 0) throw const AviaryException('keystore add failed');
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
      _lib
          .fn<_BufNodeAddrC>('aviary_keystore_reveal')
          .asFunction<_BufNodeAddrD>()(_h, p[0]),
      'keystore_reveal',
    ),
  );

  /// The persistable encrypted blob.
  Uint8List serialise() => _bc._take(
    _lib
        .fn<_BufOfHandleC>('aviary_keystore_serialise')
        .asFunction<_BufOfHandleD>()(_h),
    'keystore_serialise',
  );

  /// Recovers the master key from [passphrase]. Throws when it is wrong.
  void unlock(String passphrase) => _withUtf8(passphrase, (cp) {
    final rc = _lib
        .fn<_KsUnlockC>('aviary_keystore_unlock')
        .asFunction<_KsUnlockD>()(_h, cp);
    if (rc != 0) {
      throw const AviaryException('keystore unlock failed');
    }
  });

  /// Whether secrets can currently be revealed.
  bool get unlocked =>
      _lib
          .fn<_IntOfHandleC>('aviary_keystore_unlocked')
          .asFunction<_IntOfHandleD>()(_h) ==
      1;

  /// Releases the native store.
  void dispose() => _release();
}
