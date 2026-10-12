part of '../../aviary.dart';

/// Application records on a chain assembled with the `record` state model,
/// and the transaction queue every assembled chain proposes from.
/// {@category Records}
extension ChainRecords on Chain {
  /// Queues a signed, encoded transaction for the next proposal after checking
  /// it against the chain and the state the earlier queued transactions
  /// leave. Returns the txid; throws with the reason otherwise.
  Uint8List addTx(Uint8List tx) => _blockOp('aviary_chain_add_tx', tx);

  /// Transactions waiting for the next proposal.
  int get pendingCount => _bc.lib.lookupFunction<_U64OfHandleC, _U64OfHandleD>(
    'aviary_chain_pending_count',
  )(_h);

  /// Signs and queues a record write from a 64 byte Ed25519 [secret]; the
  /// nonce is taken from the chain, so several writes can be queued at once.
  Uint8List write(
    Uint8List secret,
    Uint8List key,
    RecordOp op,
    Uint8List value,
  ) {
    final author = _bc.addressOf(secret.sublist(32));
    return addTx(
      _bc.signRecordTx(
        chainId: chainId,
        nonce: recordNonce(author),
        key: key,
        op: op,
        value: value,
        secret: secret,
      ),
    );
  }

  /// The committed record at [key], or null when none exists.
  RecordEntry? record(Uint8List key) {
    try {
      return _decodeRecord(_byKey('aviary_chain_record', key), withKey: false)
          as RecordEntry;
    } on AviaryException catch (e) {
      if (e.message.contains('no such record')) return null;
      rethrow;
    }
  }

  /// The next nonce [address] must use, counting queued writes.
  int recordNonce(Uint8List address) => _scoped(
    [address],
    (p) => _bc.lib.lookupFunction<_RecAddrU64C, _RecAddrU64D>(
      'aviary_chain_record_nonce',
    )(_h, p[0]),
  );

  /// A state inclusion proof for the record at [key]; verify it with
  /// [AviaryProofs.verifyStateProof] and read it with [AviaryRecords.recordFromProof].
  Uint8List proveRecord(Uint8List key) =>
      _byKey('aviary_chain_prove_record', key);

  Uint8List _byKey(String fn, Uint8List key) => _scoped(
    [key],
    (p) => _bc._take(
      _bc.lib.lookupFunction<_ScChainAddrC, _ScChainAddrD>(fn)(_h, p[0]),
      fn,
    ),
  );
}
