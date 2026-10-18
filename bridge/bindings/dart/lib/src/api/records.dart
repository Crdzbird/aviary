part of '../../aviary.dart';

/// Record transactions and proofs that need no chain handle.
/// {@category Records}
extension AviaryRecords on Aviary {
  /// A signed, encoded record transaction from a 64 byte Ed25519 [secret].
  Uint8List signRecordTx({
    required int chainId,
    required int nonce,
    required Uint8List key,
    required RecordOp op,
    required Uint8List value,
    required Uint8List secret,
  }) => _scoped(
    [key, value, secret],
    (p) => _take(
      lib.fn<_RecSignC>('aviary_record_tx_sign').asFunction<_RecSignD>()(
        chainId,
        nonce,
        p[0],
        op.index,
        p[1],
        value.length,
        p[2],
        secret.length,
      ),
      'record_tx_sign',
    ),
  );

  /// A signed, encoded record transaction from a hybrid Ed25519 + ML-DSA-65
  /// account ([Node.newAccountHybrid]), in the version 2 envelope.
  Uint8List signRecordTxHybrid({
    required int chainId,
    required int nonce,
    required Uint8List key,
    required RecordOp op,
    required Uint8List value,
    required Uint8List publicKey,
    required Uint8List secret,
  }) => _scoped([key, value, publicKey, secret], (p) {
    final f = lib
        .fn<_RecSignHybC>('aviary_record_tx_sign_hybrid')
        .asFunction<_RecSignHybD>();
    final r = f(
      chainId,
      nonce,
      p[0],
      op.index,
      p[1],
      value.length,
      p[2],
      publicKey.length,
      p[3],
      secret.length,
    );
    return _take(r, 'record_tx_sign_hybrid');
  });

  /// The 32 byte chain address of [publicKey].
  Uint8List addressOf(Uint8List publicKey) => _scoped(
    [publicKey],
    (p) => _take(
      lib.fn<_BufOfBytesC>('aviary_address_of').asFunction<_BufOfBytesD>()(
        p[0],
        publicKey.length,
      ),
      'address_of',
    ),
  );

  /// The record a verified state proof commits to.
  RecordLeaf recordFromProof(Uint8List proof) => _scoped(
    [proof],
    (p) =>
        _decodeRecord(
              _take(
                lib
                    .fn<_BufOfBytesC>('aviary_record_from_proof')
                    .asFunction<_BufOfBytesD>()(p[0], proof.length),
                'record_from_proof',
              ),
              withKey: true,
            )
            as RecordLeaf,
  );
}
