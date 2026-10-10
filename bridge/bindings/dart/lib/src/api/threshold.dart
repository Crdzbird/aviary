part of '../../aviary.dart';

/// Threshold accounts: any t of n share holders can sign.
extension AviaryThreshold on Aviary {
  /// Deals a t of n account as [n] share cards. The group secret is erased.
  List<ThresholdShare> createThresholdAccount(int n, int t) {
    final blob = _take(
      lib.lookupFunction<_ThCreateC, _ThCreateD>('aviary_threshold_create')(
          n, t),
      'threshold_create',
    );
    final count = (blob[0] << 8) | blob[1];
    final out = <ThresholdShare>[];
    var off = 2;
    for (var i = 0; i < count; i++) {
      final len = _u32be(blob, off);
      off += 4;
      out.add(_shareInfo(Uint8List.sublistView(blob, off, off + len)));
      off += len;
    }
    return out;
  }

  ThresholdShare _shareInfo(Uint8List card) => _scoped([card], (p) {
        final info = _take(
          lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
              'aviary_threshold_share_info')(p[0], card.length),
          'threshold_share_info',
        );
        return ThresholdShare(
            card,
            (info[0] << 8) | info[1],
            Uint8List.sublistView(info, 2, 34),
            Uint8List.sublistView(info, 34, 66));
      });

  /// Seals a share card for rest. Zero ops and mem select the sensitive preset.
  Uint8List sealShare(Uint8List card, String passphrase,
          {int ops = 0, int mem = 0}) =>
      _withUtf8(
        passphrase,
        (pp) => _scoped(
          [card],
          (p) => _take(
            lib.lookupFunction<_ThSealC, _ThSealD>(
                'aviary_threshold_share_seal')(p[0], card.length, pp, ops, mem),
            'threshold_share_seal',
          ),
        ),
      );

  /// Opens a sealed share card. Throws on a wrong passphrase or tamper.
  Uint8List unsealShare(Uint8List envelope, String passphrase) => _withUtf8(
        passphrase,
        (pp) => _scoped(
          [envelope],
          (p) => _take(
            lib.lookupFunction<_ThUnsealC, _ThUnsealD>(
                'aviary_threshold_share_unseal')(p[0], envelope.length, pp),
            'threshold_share_unseal',
          ),
        ),
      );

  /// Round one for a card. Keep the nonces secret, share the commitment.
  ThresholdCommit commit(Uint8List card) => _scoped([card], (p) {
        final b = _take(
          lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
              'aviary_threshold_commit')(p[0], card.length),
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
      final h = lib.lookupFunction<_ThCerNewC, _ThCerNewD>(
              'aviary_threshold_ceremony_new')(
          p[0], p[1], message.length, p[2], roster.length);
      return Ceremony._(this, _requireHandle(h, 'threshold ceremony creation'));
    });
  }
}
