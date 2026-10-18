part of '../../aviary.dart';

/// Genesis construction and identity.
/// {@category Ledger}
extension AviaryGenesis on Aviary {
  /// A minimal genesis: chain params, posture, seeded state and validators.
  Uint8List buildGenesis({
    required int chainId,
    required List<GenesisAccountEntry> accounts,
    required List<ValidatorEntry> validators,
    int timestamp = 1,
    int assuranceLevel = 1,
    int stateModel = 0,
  }) => _withU64s(
    [for (final a in accounts) a.balance],
    (bal) => _withU64s(
      _powers(validators),
      (pow) => _scoped(
        [_concat(accounts.map((a) => a.address)), _publicKeys(validators)],
        (p) => _take(
          lib
              .fn<_GenesisBuildC>('aviary_genesis_build')
              .asFunction<_GenesisBuildD>()(
            chainId,
            timestamp,
            assuranceLevel,
            stateModel,
            p[0],
            bal,
            accounts.length,
            p[1],
            pow,
            validators.length,
          ),
          'genesis_build',
        ),
      ),
    ),
  );

  /// Commits a mixture into a genesis; the genesis hash changes.
  Uint8List commitBlueprint(Uint8List genesis, Blueprint bp) =>
      _buf2Op('aviary_genesis_commit_blueprint', genesis, bp.encoded);

  /// The 32 byte genesis hash, the chain identity.
  Uint8List genesisHash(Uint8List genesis) =>
      _bufOp('aviary_genesis_hash', genesis);

  Uint8List _bufOp(String fn, Uint8List a) => _scoped(
    [a],
    (p) => _take(
      lib.fn<_BufOfBytesC>(fn).asFunction<_BufOfBytesD>()(p[0], a.length),
      fn,
    ),
  );

  Uint8List _buf2Op(String fn, Uint8List a, Uint8List b) => _scoped(
    [a, b],
    (p) => _take(
      lib.fn<_Buf2C>(fn).asFunction<_Buf2D>()(p[0], a.length, p[1], b.length),
      fn,
    ),
  );
}
