part of '../../aviary.dart';

/// Light client verification of proofs and certificates.
extension AviaryProofs on Aviary {
  bool _verify(String fn, Uint8List root, Uint8List proof) => _scoped(
        [root, proof],
        (p) =>
            lib.lookupFunction<_VerifyC, _VerifyD>(fn)(
                p[0], p[1], proof.length) ==
            1,
      );

  /// Whether a state inclusion proof holds under a 32 byte state root.
  bool verifyStateProof(Uint8List stateRoot, Uint8List proof) =>
      _verify('aviary_verify_state_proof', stateRoot, proof);

  /// Whether a transaction inclusion proof holds under a 32 byte tx root.
  bool verifyTxInclusion(Uint8List txRoot, Uint8List proof) =>
      _verify('aviary_verify_tx_inclusion', txRoot, proof);

  /// Whether a commit certificate is valid under an encoded validator set.
  /// Malformed input yields false.
  bool verifyCommit(int chainId, Uint8List validatorSet, Uint8List cert) =>
      _scoped(
        [validatorSet, cert],
        (p) =>
            lib.lookupFunction<_VerifyCommitC, _VerifyCommitD>(
                    'aviary_verify_commit')(
                chainId, p[0], validatorSet.length, p[1], cert.length) ==
            1,
      );
}
