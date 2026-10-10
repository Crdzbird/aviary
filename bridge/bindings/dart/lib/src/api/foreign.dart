part of '../../aviary.dart';

/// A consensus engine implemented in Dart and plugged into a mixture by id.
/// The pipeline stays native; Dart supplies the justification policy. Both
/// methods run synchronously inside consensus, must be deterministic, and
/// must not call back into the chain.
abstract class ForeignEngine {
  /// Allows subclasses to be const.
  const ForeignEngine();

  /// The justification for a block this node proposed, at most 4096 bytes,
  /// or null to refuse.
  Uint8List? justify(Uint8List block);

  /// Whether [justification] is valid for [block].
  bool verify(Uint8List block, Uint8List justification);
}

/// Foreign components: engines and proposer policies implemented in Dart.
extension AviaryForeign on Aviary {
  /// Registers [engine] under [id]. The id is chain identity; never rename
  /// it. [grindProof] declares an unpredictable leader election.
  bool registerEngine(String id, ForeignEngine engine,
      {bool grindProof = false}) {
    final h = _nextForeignHandle++;
    _foreignEngines[h] = engine;
    final j = NativeCallable<_JustifyC>.isolateLocal(_justifyTrampoline,
        exceptionalReturn: 0)
      ..keepIsolateAlive = false;
    final v = NativeCallable<_FVerifyC>.isolateLocal(_verifyTrampoline,
        exceptionalReturn: 0)
      ..keepIsolateAlive = false;
    _foreignCallables
      ..add(j)
      ..add(v);
    return _withUtf8(
      id,
      (cid) =>
          lib.lookupFunction<_RegEngineC, _RegEngineD>(
                  'aviary_registry_add_engine')(
              cid,
              j.nativeFunction,
              v.nativeFunction,
              Pointer<Void>.fromAddress(h),
              grindProof ? 1 : 0) ==
          1,
    );
  }

  /// Registers a deterministic proposer [policy] under [id].
  bool registerProposer(String id, ProposerPolicy policy) {
    final h = _nextForeignHandle++;
    _foreignProposers[h] = policy;
    final p = NativeCallable<_ProposerC>.isolateLocal(_proposerTrampoline,
        exceptionalReturn: 0)
      ..keepIsolateAlive = false;
    _foreignCallables.add(p);
    return _withUtf8(
      id,
      (cid) =>
          lib.lookupFunction<_RegProposerC, _RegProposerD>(
                  'aviary_registry_add_proposer')(
              cid, p.nativeFunction, Pointer<Void>.fromAddress(h)) ==
          1,
    );
  }
}
