part of '../../aviary.dart';

/// Portable account import: opening a sealed account.
/// {@category Custody}
extension AviaryPortableOpen on Aviary {
  PortableAccount _account(Uint8List blob) {
    final kp = _splitKeyBlob(blob);
    return PortableAccount(kp.public, kp.secret);
  }

  /// Opens a passphrase envelope. Throws on a wrong passphrase or tamper.
  PortableAccount openAccountPassphrase(
    Uint8List envelope,
    String passphrase,
  ) => _withUtf8(
    passphrase,
    (cp) => _scoped(
      [envelope],
      (p) => _account(
        _take(
          lib
              .fn<_POpenPassC>('aviary_portable_open_passphrase')
              .asFunction<_POpenPassD>()(p[0], envelope.length, cp),
          'portable_open_passphrase',
        ),
      ),
    ),
  );

  /// Opens a recipient bound envelope with the device's KEM secret.
  PortableAccount openAccountTo(
    Uint8List envelope,
    Uint8List recipientKemSecret,
  ) => _scoped(
    [envelope, recipientKemSecret],
    (p) => _account(
      _take(
        lib.fn<_Buf2C>('aviary_portable_open_to').asFunction<_Buf2D>()(
          p[0],
          envelope.length,
          p[1],
          recipientKemSecret.length,
        ),
        'portable_open_to',
      ),
    ),
  );

  /// Opens a signed recipient envelope: decrypts with the device KEM secret
  /// and verifies the owner's signature.
  PortableAccount openAccountSignedFrom(
    Uint8List envelope,
    Uint8List recipientKemSecret,
    Uint8List senderSigPublic,
  ) => _scoped(
    [envelope, recipientKemSecret, senderSigPublic],
    (p) => _account(
      _take(
        lib
            .fn<_POpenSignedC>('aviary_portable_open_signed_from')
            .asFunction<_POpenSignedD>()(
          p[0],
          envelope.length,
          p[1],
          recipientKemSecret.length,
          p[2],
          senderSigPublic.length,
        ),
        'portable_open_signed_from',
      ),
    ),
  );
}
