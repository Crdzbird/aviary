part of '../../aviary.dart';

/// Portable account export: sealing an account for transport.
/// {@category Custody}
extension AviaryPortableSeal on Aviary {
  /// A hybrid KEM device key pair for the recipient bound tiers.
  PortableAccount hybridKemKeypair() {
    final kp = _splitKeyBlob(
      _take(
        lib.lookupFunction<_KemKeygenC, _KemKeygenD>(
          'aviary_hybrid_kem_keygen',
        )(),
        'hybrid_kem_keygen',
      ),
    );
    return PortableAccount(kp.public, kp.secret);
  }

  /// Tier one: seal an account under a passphrase. Zero ops and mem select
  /// the sensitive preset.
  Uint8List sealAccountPassphrase(
    Uint8List public,
    Uint8List secret,
    String passphrase, {
    int ops = 0,
    int mem = 0,
  }) => _withUtf8(
    passphrase,
    (cp) => _scoped(
      [public, secret],
      (p) => _take(
        lib.lookupFunction<_PSealPassC, _PSealPassD>(
          'aviary_portable_seal_passphrase',
        )(p[0], public.length, p[1], secret.length, cp, ops, mem),
        'portable_seal_passphrase',
      ),
    ),
  );

  /// Tier two: seal an account to a device's hybrid KEM public key.
  Uint8List sealAccountTo(
    Uint8List public,
    Uint8List secret,
    Uint8List recipientKemPublic,
  ) => _scoped(
    [public, secret, recipientKemPublic],
    (p) => _take(
      lib.lookupFunction<_PSealToC, _PSealToD>('aviary_portable_seal_to')(
        p[0],
        public.length,
        p[1],
        secret.length,
        p[2],
        recipientKemPublic.length,
      ),
      'portable_seal_to',
    ),
  );

  /// Tier three: seal to a device KEM key and sign with the owner's hybrid
  /// signature secret.
  Uint8List sealAccountSignedTo(
    Uint8List public,
    Uint8List secret,
    Uint8List recipientKemPublic,
    Uint8List senderSigSecret,
  ) => _scoped(
    [public, secret, recipientKemPublic, senderSigSecret],
    (p) => _take(
      lib.lookupFunction<_PSealSignedC, _PSealSignedD>(
        'aviary_portable_seal_signed_to',
      )(
        p[0],
        public.length,
        p[1],
        secret.length,
        p[2],
        recipientKemPublic.length,
        p[3],
        senderSigSecret.length,
      ),
      'portable_seal_signed_to',
    ),
  );
}
