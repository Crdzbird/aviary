part of '../../aviary.dart';

/// Assurance postures and the sealed mempool.
/// {@category Assurance and credentials}
extension AviaryAssurance on Aviary {
  /// Describes the posture at [level]: 1 standard, 2 hardened, 3 fortress.
  AssuranceProfile describeAssurance(int level) {
    final b = _take(
      lib
          .fn<_AsDescribeC>('aviary_assurance_describe')
          .asFunction<_AsDescribeD>()(level),
      'assurance_describe',
    );
    return AssuranceProfile(
      ratcheted: b[0] == 1,
      vrf: b[1] == 1,
      thresholdCustody: b[2] == 1,
      sealedMempool: b[3] == 1,
      sigAlgos: Uint8List.sublistView(b, 5, 5 + b[4]),
    );
  }

  /// Canonically encodes a transaction from its fields.
  Uint8List encodeTx({
    required int chainId,
    required Uint8List body,
    required Uint8List author,
    required Uint8List signature,
    int version = 2,
    int sigAlgo = 1,
    int fee = 0,
  }) => _scoped(
    [body, author, signature],
    (p) => _take(
      lib.fn<_TxEncodeC>('aviary_tx_encode').asFunction<_TxEncodeD>()(
        version,
        chainId,
        sigAlgo,
        fee,
        p[0],
        body.length,
        p[1],
        author.length,
        p[2],
        signature.length,
      ),
      'tx_encode',
    ),
  );

  /// Seals an encoded transaction to a proposer's KEM public key.
  Uint8List sealTx(Uint8List txEncoded, Uint8List proposerKemPublic) => _scoped(
    [txEncoded, proposerKemPublic],
    (p) => _take(
      lib.fn<_Buf2C>('aviary_sealed_tx_seal').asFunction<_Buf2D>()(
        p[0],
        txEncoded.length,
        p[1],
        proposerKemPublic.length,
      ),
      'sealed_tx_seal',
    ),
  );

  /// Opens a sealed mempool with room for [capacity] transactions.
  SealedMempool makeSealedMempool(int capacity) {
    final h =
        lib.fn<_SmNewC>('aviary_sealed_mempool_new').asFunction<_SmNewD>()(
          capacity,
        );
    return SealedMempool._(this, _requireHandle(h, 'sealed mempool creation'));
  }
}
