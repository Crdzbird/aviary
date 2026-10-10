part of '../../aviary.dart';

/// A confidential, mutually authenticated, post quantum hybrid link between
/// two peers. Create one with [AviaryNetwork.initiateSecureSession] or
/// [AviaryNetwork.acceptSecureSession], exchange the handshake messages, then
/// wrap every frame with [encrypt] and [decrypt]. Always check
/// [peerIdentity] against the peers you expect: the handshake proves the
/// peer holds that key, not that it is the peer you wanted.
final class SecureSession extends _NativeHandle {
  SecureSession._(Aviary bc, Pointer<Void> h)
      : super(bc, h, 'aviary_secure_free');

  Uint8List _buf(String fn) => _bc._take(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(fn)(_h), fn);

  Uint8List _crypt(String fn, Uint8List input) => _scoped(
        [input],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_ChainBlockC, _ChainBlockD>(fn)(
              _h, p[0], input.length),
          fn,
        ),
      );

  /// The handshake message this side must send: the first message for an
  /// initiator, the reply for a responder.
  Uint8List get handshakeMessage => _buf('aviary_secure_handshake_message');

  /// Initiator only: consumes the responder's reply. Throws with
  /// [AviaryStatus.badInput] when the responder does not verify.
  void complete(Uint8List reply) {
    final ok = _scoped(
      [reply],
      (p) => _bc.lib.lookupFunction<_HandleBytesIntC, _HandleBytesIntD>(
          'aviary_secure_complete')(_h, p[0], reply.length),
    );
    if (ok != 1) {
      throw const AviaryException(
          'secure session: responder refused', AviaryStatus.badInput);
    }
  }

  /// Whether both directions are ready.
  bool get isOpen =>
      _bc.lib.lookupFunction<_HandleFlagC, _HandleFlagD>('aviary_secure_open')(
          _h) ==
      1;

  /// The authenticated remote signing public key; empty until open.
  Uint8List get peerIdentity => _buf('aviary_secure_peer_identity');

  /// Encrypts one frame for the peer.
  Uint8List encrypt(Uint8List plaintext) =>
      _crypt('aviary_secure_encrypt', plaintext);

  /// Decrypts one frame from the peer. Throws on tamper or reorder.
  Uint8List decrypt(Uint8List frame) => _crypt('aviary_secure_decrypt', frame);

  /// Releases the native session.
  void free() => _release();
}
