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
  }) =>
      _onWorker((bc) => bc.sealShare(card, passphrase, ops: ops, mem: mem));

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
  }) =>
      _onWorker((bc) => bc.sealAccountPassphrase(public, secret, passphrase,
          ops: ops, mem: mem));

  /// [AviaryPortableOpen.openAccountPassphrase] on a worker isolate.
  Future<PortableAccount> openAccountPassphraseAsync(
    Uint8List envelope,
    String passphrase,
  ) =>
      _onWorker((bc) => bc.openAccountPassphrase(envelope, passphrase));

  Future<R> _onWorker<R>(R Function(Aviary engine) body) {
    final path = _path;
    return Isolate.run(
      () => body(path == null ? Aviary.instance : Aviary.load(path)),
    );
  }
}
