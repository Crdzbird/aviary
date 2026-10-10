part of '../../blockchainer.dart';

/// A whole threshold signing ceremony run in one process.
extension ThresholdSigning on Blockchainer {
  /// Signs [message] with [shares], at least the account's threshold of
  /// them, and returns the 64 byte Ed25519 signature. Every share commits,
  /// signs and is attributed; a share whose signature does not verify
  /// throws with its index.
  Uint8List signWithShares(List<ThresholdShare> shares, Uint8List message) {
    if (shares.isEmpty) throw ArgumentError.value(shares, 'shares', 'empty');
    final commits = [for (final s in shares) commit(s.bytes)];
    final ceremony = makeCeremony(
      shares.first.groupPublicKey,
      message,
      [for (final c in commits) c.commitment],
    );
    try {
      final signatures = <Uint8List>[];
      for (var i = 0; i < shares.length; i++) {
        final share = shares[i];
        final signature = ceremony.sign(share.bytes, commits[i].nonces);
        if (!ceremony.verifyShare(share.index, share.publicShare, signature)) {
          throw BlockchainerException('share ${share.index} did not verify');
        }
        signatures.add(signature);
      }
      return ceremony.aggregate([for (final s in shares) s.index], signatures);
    } finally {
      ceremony.free();
    }
  }
}
