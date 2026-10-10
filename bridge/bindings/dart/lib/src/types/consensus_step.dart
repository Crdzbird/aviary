part of '../../aviary.dart';

/// The step of a consensus round a timeout belongs to.
enum ConsensusStep {
  /// Waiting for the proposer's block.
  propose(0),

  /// Waiting for prevotes.
  prevote(1),

  /// Waiting for precommits.
  precommit(2);

  const ConsensusStep(this.code);

  /// The ABI value.
  final int code;

  /// The step for an ABI [code]; unknown codes map to [propose].
  static ConsensusStep fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => propose);
}
