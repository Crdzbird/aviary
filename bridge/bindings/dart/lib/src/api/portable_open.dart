part of '../../aviary.dart';

/// Portable account import: opening a sealed account.
extension AviaryPortableOpen on Aviary {
  PortableAccount _account(Uint8List blob) {
    final kp = _splitKeyBlob(blob);
    return PortableAccount(kp.public, kp.secret);
  }

  /// Opens a passphrase envelope. Throws on a wrong passphrase or tamper.
  PortableAccount openAccountPassphrase(
          Uint8List envelope, String passphrase) =>
      _withUtf8(
        passphrase,
        (cp) => _scoped(
          [envelope],
          (p) => _account(_take(
            lib.lookupFunction<_POpenPassC, _POpenPassD>(
                'aviary_portable_open_passphrase')(p[0], envelope.length, cp),
            'portable_open_passphrase',
          )),
        ),
      );

  /// Opens a recipient bound envelope with the device's KEM secret.
  PortableAccount openAccountTo(
          Uint8List envelope, Uint8List recipientKemSecret) =>
      _scoped(
        [envelope, recipientKemSecret],
        (p) => _account(_take(
          lib.lookupFunction<_Buf2C, _Buf2D>('aviary_portable_open_to')(
              p[0], envelope.length, p[1], recipientKemSecret.length),
          'portable_open_to',
        )),
      );

  /// Opens a signed recipient envelope: decrypts with the device KEM secret
  /// and verifies the owner's signature.
  PortableAccount openAccountSignedFrom(
    Uint8List envelope,
    Uint8List recipientKemSecret,
    Uint8List senderSigPublic,
  ) =>
      _scoped(
        [envelope, recipientKemSecret, senderSigPublic],
        (p) => _account(_take(
          lib.lookupFunction<_POpenSignedC, _POpenSignedD>(
                  'aviary_portable_open_signed_from')(p[0], envelope.length,
              p[1], recipientKemSecret.length, p[2], senderSigPublic.length),
          'portable_open_signed_from',
        )),
      );
}
