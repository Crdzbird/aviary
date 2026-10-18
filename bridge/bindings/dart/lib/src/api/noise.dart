part of '../../aviary.dart';

/// A Noise identity: the X25519 static pair and the hybrid signature pair.
/// {@category Networking}
typedef NoiseIdentity = ({KeyPairBytes noiseStatic, KeyPairBytes sig});

/// Noise XX sessions with a post quantum identity proof, over a transport
/// the app owns.
/// {@category Networking}
extension AviaryNoise on Aviary {
  /// A fresh Noise identity.
  NoiseIdentity generateNoiseIdentity() {
    final p = _splitBlobList(
      _take(
        lib.lookupFunction<_BufNoArgC, _BufNoArgD>(
          'aviary_noise_identity_generate',
        )(),
        'noise_identity_generate',
      ),
    );
    return (
      noiseStatic: (public: p[0], secret: p[1]),
      sig: (public: p[2], secret: p[3]),
    );
  }

  /// The dialing side: its [NoiseSession.handshakeMessage] is msg0.
  NoiseSession initiateNoiseSession(NoiseIdentity me, {Uint8List? prologue}) {
    final id = _noiseBlob(me), pro = prologue ?? Uint8List(0);
    final h = _scoped(
      [id, pro],
      (p) => lib.lookupFunction<_NoiseInitC, _NoiseInitD>(
        'aviary_noise_initiate',
      )(p[0], id.length, p[1], pro.length),
    );
    return NoiseSession._(this, _requireHandle(h, 'noise initiate'));
  }

  /// The accepting side, from the initiator's msg0; its handshakeMessage is msg1.
  NoiseSession acceptNoiseSession(
    NoiseIdentity me,
    Uint8List msg0, {
    Uint8List? prologue,
  }) {
    final id = _noiseBlob(me), pro = prologue ?? Uint8List(0);
    final h = _scoped(
      [id, msg0, pro],
      (p) => lib.lookupFunction<_NoiseAcceptC, _NoiseAcceptD>(
        'aviary_noise_accept',
      )(p[0], id.length, p[1], msg0.length, p[2], pro.length),
    );
    return NoiseSession._(this, _requireHandle(h, 'noise accept'));
  }

  Uint8List _noiseBlob(NoiseIdentity me) => _blobList([
    me.noiseStatic.public,
    me.noiseStatic.secret,
    me.sig.public,
    me.sig.secret,
  ]);
}
