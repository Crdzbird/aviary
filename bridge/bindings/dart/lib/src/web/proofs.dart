part of '../aviary_web.dart';

/// Light client verification of proofs and certificates.
/// {@category Light clients}
extension AviaryProofs on Aviary {
  bool _verify(String fn, Uint8List root, Uint8List proof) {
    if (root.length != 32) return false;
    return _engine.scoped(
      [root, proof],
      (p) => _engine.call(fn, [p[0], p[1], proof.length]) == 1,
    );
  }

  /// Whether a state inclusion proof holds under a 32 byte state root.
  bool verifyStateProof(Uint8List stateRoot, Uint8List proof) =>
      _verify('aviary_verify_state_proof', stateRoot, proof);

  /// Whether a transaction inclusion proof holds under a 32 byte tx root.
  bool verifyTxInclusion(Uint8List txRoot, Uint8List proof) =>
      _verify('aviary_verify_tx_inclusion', txRoot, proof);

  /// Whether a commit certificate is valid under an encoded validator set.
  /// Malformed input yields false.
  bool verifyCommit(int chainId, Uint8List validatorSet, Uint8List cert) =>
      _engine.scoped(
        [validatorSet, cert],
        (p) =>
            _engine.call('aviary_web_verify_commit', [
              ..._halves(chainId, 'chainId'),
              p[0],
              validatorSet.length,
              p[1],
              cert.length,
            ]) ==
            1,
      );
}
