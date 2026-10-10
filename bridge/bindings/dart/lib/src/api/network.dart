part of '../../aviary.dart';

/// Consensus over a network the app owns: nodes that return what to send and
/// what to arm, and the authenticated session that protects each link.
extension AviaryNetwork on Aviary {
  /// The validator index that follows a chain without voting.
  static const int observer = 0xFFFFFFFF;

  /// A fresh long term identity for [acceptSecureSession] and the signature
  /// half of [initiateSecureSession].
  SecureIdentity generateSecureIdentity() {
    final parts = _splitBlobList(
      _take(
        lib.lookupFunction<_BufNoArgC, _BufNoArgD>(
          'aviary_secure_identity_generate',
        )(),
        'secure_identity_generate',
      ),
    );
    return (
      kem: (public: parts[0], secret: parts[1]),
      sig: (public: parts[2], secret: parts[3]),
    );
  }

  /// The dialing side of a link: encapsulates to the responder's KEM public
  /// key and authenticates with [sig]. Send [SecureSession.handshakeMessage]
  /// and feed the reply to [SecureSession.complete].
  SecureSession initiateSecureSession(
    Uint8List responderKemPublic,
    KeyPairBytes sig,
  ) => _scoped([responderKemPublic, sig.public, sig.secret], (p) {
    final h =
        lib.lookupFunction<_SecInitC, _SecInitD>('aviary_secure_initiate')(
          p[0],
          responderKemPublic.length,
          p[1],
          sig.public.length,
          p[2],
          sig.secret.length,
        );
    return SecureSession._(this, _requireHandle(h, 'secure initiate'));
  });

  /// The accepting side of a link: consumes the initiator's [firstMessage]
  /// under [me]. The session is open at once; send its handshakeMessage back.
  /// Throws with [AviaryStatus.badInput] when the initiator does not verify.
  SecureSession acceptSecureSession(SecureIdentity me, Uint8List firstMessage) {
    final k = me.kem;
    final g = me.sig;
    final f = lib.lookupFunction<_SecAcceptC, _SecAcceptD>(
      'aviary_secure_accept',
    );
    final parts = [k.public, k.secret, g.public, g.secret, firstMessage];
    final n = parts.map((b) => b.length).toList();
    return _scoped(parts, (p) {
      final h = f(p[0], n[0], p[1], n[1], p[2], n[2], p[3], n[3], p[4], n[4]);
      if (h == nullptr) {
        throw const AviaryException(
          'secure session refused',
          AviaryStatus.badInput,
        );
      }
      return SecureSession._(this, h);
    });
  }

  /// A consensus node for validator [me] of [genesis], or an observer when
  /// [me] is [observer]. [peers] are the mesh neighbours to relay to.
  NetNode newNetNode({
    required Uint8List genesis,
    required int me,
    KeyPairBytes? blsKey,
    int timestamp = 1,
    List<int> peers = const [],
  }) {
    final pub = blsKey?.public ?? Uint8List(0);
    final sec = blsKey?.secret ?? Uint8List(0);
    final node = _scoped([genesis, pub, sec], (p) {
      final h = lib.lookupFunction<_NetNewC, _NetNewD>('aviary_net_node_new')(
        p[0],
        genesis.length,
        me,
        p[1],
        pub.length,
        p[2],
        sec.length,
        timestamp,
      );
      return NetNode._(this, _requireHandle(h, 'net node creation'));
    });
    peers.forEach(node.addPeer);
    return node;
  }
}
