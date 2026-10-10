part of '../../aviary.dart';

/// The Argon2 bound custody operations on a worker isolate, so the calling
/// isolate stays responsive. The worker loads the engine the way this
/// instance was loaded: the same explicit path, or the platform default.
extension AviaryAsync on Aviary {
  /// [AviaryThreshold.sealShare] on a worker isolate.
  Future<Uint8List> sealShareAsync(
    Uint8List card,
    String passphrase, {
    int ops = 0,
    int mem = 0,
  }) => _onWorker((bc) => bc.sealShare(card, passphrase, ops: ops, mem: mem));

  /// [AviaryThreshold.unsealShare] on a worker isolate.
  Future<Uint8List> unsealShareAsync(Uint8List envelope, String passphrase) =>
      _onWorker((bc) => bc.unsealShare(envelope, passphrase));

  /// [AviaryPortableSeal.sealAccountPassphrase] on a worker isolate.
  Future<Uint8List> sealAccountPassphraseAsync(
    Uint8List public,
    Uint8List secret,
    String passphrase, {
    int ops = 0,
    int mem = 0,
  }) => _onWorker(
    (bc) => bc.sealAccountPassphrase(
      public,
      secret,
      passphrase,
      ops: ops,
      mem: mem,
    ),
  );

  /// [AviaryPortableOpen.openAccountPassphrase] on a worker isolate.
  Future<PortableAccount> openAccountPassphraseAsync(
    Uint8List envelope,
    String passphrase,
  ) => _onWorker((bc) => bc.openAccountPassphrase(envelope, passphrase));

  Future<R> _onWorker<R>(R Function(Aviary engine) body) {
    final path = _path;
    return Isolate.run(
      () => body(path == null ? Aviary.instance : Aviary.load(path)),
    );
  }
}

/// A whole threshold signing ceremony run in one process.
extension ThresholdSigning on Aviary {
  /// Signs [message] with [shares], at least the account's threshold of
  /// them, and returns the 64 byte Ed25519 signature. Every share commits,
  /// signs and is attributed; a share whose signature does not verify
  /// throws with its index.
  Uint8List signWithShares(List<ThresholdShare> shares, Uint8List message) {
    if (shares.isEmpty) throw ArgumentError.value(shares, 'shares', 'empty');
    final commits = [for (final s in shares) commit(s.bytes)];
    final ceremony = makeCeremony(shares.first.groupPublicKey, message, [
      for (final c in commits) c.commitment,
    ]);
    try {
      final signatures = <Uint8List>[];
      for (var i = 0; i < shares.length; i++) {
        final share = shares[i];
        final signature = ceremony.sign(share.bytes, commits[i].nonces);
        if (!ceremony.verifyShare(share.index, share.publicShare, signature)) {
          throw AviaryException('share ${share.index} did not verify');
        }
        signatures.add(signature);
      }
      return ceremony.aggregate([for (final s in shares) s.index], signatures);
    } finally {
      ceremony.free();
    }
  }
}
