part of '../../aviary.dart';

/// Threshold accounts: any t of n share holders can sign.
/// {@category Custody}
extension AviaryThreshold on Aviary {
  /// Deals a t of n account as [n] share cards. The group secret is erased.
  List<ThresholdShare> createThresholdAccount(int n, int t) {
    final blob = _take(
      lib.fn<_ThCreateC>('aviary_threshold_create').asFunction<_ThCreateD>()(
        n,
        t,
      ),
      'threshold_create',
    );
    final count = (blob[0] << 8) | blob[1];
    final out = <ThresholdShare>[];
    var off = 2;
    for (var i = 0; i < count; i++) {
      final len = _u32be(blob, off);
      off += 4;
      out.add(_shareInfo(this, Uint8List.sublistView(blob, off, off + len)));
      off += len;
    }
    return out;
  }

  /// Seals a share card for rest. Zero ops and mem select the sensitive preset.
  Uint8List sealShare(
    Uint8List card,
    String passphrase, {
    int ops = 0,
    int mem = 0,
  }) => _withUtf8(
    passphrase,
    (pp) => _scoped(
      [card],
      (p) => _take(
        lib.fn<_ThSealC>('aviary_threshold_share_seal').asFunction<_ThSealD>()(
          p[0],
          card.length,
          pp,
          ops,
          mem,
        ),
        'threshold_share_seal',
      ),
    ),
  );

  /// Opens a sealed share card. Throws on a wrong passphrase or tamper.
  Uint8List unsealShare(Uint8List envelope, String passphrase) {
    final f = lib.fn<_ThUnsealC>('aviary_threshold_share_unseal');
    final g = f.asFunction<_ThUnsealD>();
    return _withUtf8(passphrase, (pp) {
      return _scoped([envelope], (p) {
        return _take(g(p[0], envelope.length, pp), 'threshold_share_unseal');
      });
    });
  }

  /// Round one for a card. Keep the nonces secret, share the commitment.
  ThresholdCommit commit(Uint8List card) => _scoped([card], (p) {
    final f = lib.fn<_BufOfBytesC>('aviary_threshold_commit');
    final b = _take(
      f.asFunction<_BufOfBytesD>()(p[0], card.length),
      'threshold_commit',
    );
    return (
      nonces: Uint8List.sublistView(b, 0, 64),
      commitment: Uint8List.sublistView(b, 64, 130),
    );
  });

  /// Opens a signing ceremony over [message] and the round one commitments.
  Ceremony makeCeremony(
    Uint8List groupPublicKey,
    Uint8List message,
    List<Uint8List> commitments,
  ) {
    final roster = _concat(commitments);
    return _scoped([groupPublicKey, message, roster], (p) {
      final f = lib.fn<_ThCerNewC>('aviary_threshold_ceremony_new');
      final g = f.asFunction<_ThCerNewD>();
      final h = g(p[0], p[1], message.length, p[2], roster.length);
      return Ceremony._(this, _requireHandle(h, 'threshold ceremony creation'));
    });
  }
}
