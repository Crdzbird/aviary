part of '../../aviary.dart';

/// Canonical bytes that are identical across every binding.
/// {@category Ledger}
extension AviaryCanonical on Aviary {
  /// The 32 byte BLAKE3 digest of [msg].
  Uint8List hash(Uint8List msg) => _scoped(
    [msg],
    (p) => _take(
      lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('aviary_hash')(
        p[0],
        msg.length,
      ),
      'hash',
    ),
  );

  /// The canonical 32 byte transaction id of the given fields.
  Uint8List txid(
    int version,
    int chainId,
    Uint8List body,
    Uint8List author,
    Uint8List signature,
  ) => _scoped(
    [body, author, signature],
    (p) => _take(
      lib.lookupFunction<_TxidC, _TxidD>('aviary_txid')(
        version,
        chainId,
        p[0],
        body.length,
        p[1],
        author.length,
        p[2],
        signature.length,
      ),
      'txid',
    ),
  );

  /// The canonical 32 byte block header hash. The three roots are 32 bytes.
  Uint8List headerHash(
    int version,
    int chainId,
    int height,
    Uint8List parentHash,
    int timestamp,
    Uint8List txRoot,
    Uint8List stateRoot,
    Uint8List consensus,
  ) => _scoped(
    [parentHash, txRoot, stateRoot, consensus],
    (p) => _take(
      lib.lookupFunction<_HeaderC, _HeaderD>('aviary_header_hash')(
        version,
        chainId,
        height,
        p[0],
        timestamp,
        p[1],
        p[2],
        p[3],
        consensus.length,
      ),
      'headerHash',
    ),
  );
}

/// Light client verification of proofs and certificates.
/// {@category Light clients}
extension AviaryProofs on Aviary {
  bool _verify(String fn, Uint8List root, Uint8List proof) => _scoped(
    [root, proof],
    (p) =>
        lib.lookupFunction<_VerifyC, _VerifyD>(fn)(p[0], p[1], proof.length) ==
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
              'aviary_verify_commit',
            )(chainId, p[0], validatorSet.length, p[1], cert.length) ==
            1,
      );
}
