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
      lib.lookupFunction<_RecSignC, _RecSignD>('aviary_record_tx_sign')(
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

  /// The 32 byte chain address of [publicKey].
  Uint8List addressOf(Uint8List publicKey) => _scoped(
    [publicKey],
    (p) => _take(
      lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>('aviary_address_of')(
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
                lib.lookupFunction<_BufOfBytesC, _BufOfBytesD>(
                  'aviary_record_from_proof',
                )(p[0], proof.length),
                'record_from_proof',
              ),
              withKey: true,
            )
            as RecordLeaf,
  );
}

Object _decodeRecord(Uint8List b, {required bool withKey}) {
  var o = 0;
  Uint8List take(int n) {
    final out = b.sublist(o, o + n);
    o += n;
    return out;
  }

  final key = withKey ? take(32) : null;
  final owner = take(32);
  final version = ByteData.sublistView(b, o, o + 8).getUint64(0);
  o += 8;
  final len = ByteData.sublistView(b, o, o + 4).getUint32(0);
  o += 4;
  final value = take(len);
  if (withKey) return (key: key!, owner: owner, version: version, value: value);
  return (owner: owner, version: version, value: value);
}

/// What a record transaction does: write, hand over to a new owner, or remove.
/// {@category Records}
enum RecordOp { put, transfer, delete }

/// A committed application record: owner, write count and current bytes.
/// {@category Records}
typedef RecordEntry = ({Uint8List owner, int version, Uint8List value});

/// A record read out of a verified state proof, with its key.
/// {@category Records}
typedef RecordLeaf = ({
  Uint8List key,
  Uint8List owner,
  int version,
  Uint8List value,
});
