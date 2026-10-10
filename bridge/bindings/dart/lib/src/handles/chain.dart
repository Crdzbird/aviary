part of '../../aviary.dart';

/// An assembled node behind the uniform propose, justify, submit flow.
final class Chain extends _NativeHandle {
  Chain._(Aviary bc, Pointer<Void> h) : super(bc, h, 'aviary_chain_free');

  /// The chain height.
  int get height => _bc.lib
      .lookupFunction<_U64OfHandleC, _U64OfHandleD>('aviary_chain_height')(_h);

  Uint8List _buf(String fn) => _bc._take(
      _bc.lib.lookupFunction<_BufOfHandleC, _BufOfHandleD>(fn)(_h), fn);

  Uint8List _blockOp(String fn, Uint8List block) => _scoped(
        [block],
        (p) => _bc._take(
          _bc.lib.lookupFunction<_ChainBlockC, _ChainBlockD>(fn)(
              _h, p[0], block.length),
          fn,
        ),
      );

  /// The encoded mixture this chain runs.
  Uint8List blueprint() => _buf('aviary_chain_blueprint');

  /// Proposes the next block. Throws when this node may not propose now.
  Uint8List propose(int timestamp) => _bc._take(
        _bc.lib.lookupFunction<_ChainProposeC, _ChainProposeD>(
            'aviary_chain_propose')(_h, timestamp),
        'chain_propose',
      );

  /// The leader justification for engines that seal; empty otherwise.
  Uint8List seal(Uint8List block) => _blockOp('aviary_chain_seal', block);

  /// Proof of work: the solved block.
  Uint8List mine(Uint8List block) => _blockOp('aviary_chain_mine', block);

  /// BFT: this validator's precommit as a commit certificate.
  Uint8List certify(Uint8List block) => _blockOp('aviary_chain_certify', block);

  /// Verifies [justification] and applies [block] atomically.
  bool submit(Uint8List block, [Uint8List? justification]) {
    final j = justification ?? Uint8List(0);
    return _scoped(
      [block, j],
      (p) =>
          _bc.lib.lookupFunction<_ChainSubmitC, _ChainSubmitD>(
              'aviary_chain_submit')(_h, p[0], block.length, p[1], j.length) ==
          1,
    );
  }

  /// BFT: the validator set in force for the next block.
  List<ValidatorEntry> validators() =>
      _decodeValidatorSet(_buf('aviary_chain_validators'));

  /// BFT: schedules a coordinated validator set change that the next
  /// proposal announces. Throws when not BFT or malformed.
  void scheduleValidators(List<ValidatorEntry> validators) => _withU64s(
        _powers(validators),
        (pw) => _scoped([_publicKeys(validators)], (p) {
          final rc = _bc.lib.lookupFunction<_ScSchedC, _ScSchedD>(
                  'aviary_chain_schedule_validators')(
              _h, p[0], pw, validators.length);
          if (rc != 1) throw StateError('chain: cannot schedule validators');
        }),
      );

  /// Releases the native chain.
  void free() => _release();
}
