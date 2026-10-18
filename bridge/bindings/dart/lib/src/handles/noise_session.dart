part of '../../aviary.dart';

/// A Noise_XX_25519_ChaChaPoly_SHA256 link for peers that speak Noise, with
/// a post quantum identity proof bound to the handshake hash. The initiator
/// sends [handshakeMessage], the responder answers with its own, the
/// initiator returns [completeInitiator]'s result, then each side sends
/// [identityProof] and checks the other's with [verifyIdentityProof].
/// Authentication is post quantum; confidentiality is X25519, so use
/// [SecureSession] when post quantum confidentiality is required. Always pin
/// [peerIdentity] against the keys you expect.
/// {@category Networking}
final class NoiseSession extends _NativeHandle {
  NoiseSession._(Aviary bc, Pointer<Void> h)
    : super(bc, h, 'aviary_noise_free');

  Uint8List _buf(String fn) => _bc._take(
    _bc.lib.fn<_BufOfHandleC>(fn).asFunction<_BufOfHandleD>()(_h),
    fn,
  );

  Uint8List _op(String fn, Uint8List input) => _scoped(
    [input],
    (p) => _bc._take(
      _bc.lib.fn<_ChainBlockC>(fn).asFunction<_ChainBlockD>()(
        _h,
        p[0],
        input.length,
      ),
      fn,
    ),
  );

  void _check(String fn, Uint8List input, String what) {
    final rc = _scoped(
      [input],
      (p) => _bc.lib.fn<_HandleBytesIntC>(fn).asFunction<_HandleBytesIntD>()(
        _h,
        p[0],
        input.length,
      ),
    );
    if (rc == 0) return;
    throw AviaryException('noise session: $what', AviaryStatus.badInput);
  }

  /// The message this side produced when it was created: msg0 for an
  /// initiator, msg1 for a responder.
  Uint8List get handshakeMessage => _buf('aviary_noise_handshake_message');

  /// Initiator: consumes msg1 and returns msg2 to send.
  Uint8List completeInitiator(Uint8List msg1) =>
      _op('aviary_noise_complete_initiator', msg1);

  /// Responder: consumes msg2.
  void completeResponder(Uint8List msg2) =>
      _check('aviary_noise_complete_responder', msg2, 'handshake refused');

  /// This side's identity proof, sent once after the handshake.
  Uint8List identityProof() => _buf('aviary_noise_identity_proof');

  /// Verifies the peer's identity proof. Throws when it does not verify.
  void verifyIdentityProof(Uint8List proof) => _check(
    'aviary_noise_verify_identity_proof',
    proof,
    'identity proof refused',
  );

  /// Whether the handshake is done and the peer's proof verified.
  bool get isOpen =>
      _bc.lib.fn<_HandleFlagC>('aviary_noise_open').asFunction<_HandleFlagD>()(
        _h,
      ) ==
      1;

  /// The peer's authenticated hybrid signature key.
  Uint8List get peerIdentity => _buf('aviary_noise_peer_identity');

  /// The peer's X25519 static key from the handshake.
  Uint8List get peerStatic => _buf('aviary_noise_peer_static');

  /// The final handshake hash, equal on both sides.
  Uint8List get handshakeHash => _buf('aviary_noise_handshake_hash');

  /// Encrypts one record. Throws until [isOpen].
  Uint8List encrypt(Uint8List plaintext) =>
      _op('aviary_noise_encrypt', plaintext);

  /// Decrypts one record. Throws on tamper, reorder, or before [isOpen].
  Uint8List decrypt(Uint8List record) => _op('aviary_noise_decrypt', record);

  /// Releases the native session.
  void free() => _release();
}
