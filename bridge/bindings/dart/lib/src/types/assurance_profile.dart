part of '../../aviary.dart';

/// The security posture a chain commits to at genesis. Higher tiers only
/// ever tighten the tier below them.
/// {@category Assurance and credentials}
abstract final class AssuranceProfile {
  /// Describes a posture from its tiers and admitted algorithms.
  const factory AssuranceProfile({
    required bool ratcheted,
    required bool vrf,
    required bool thresholdCustody,
    required bool sealedMempool,
    required Uint8List sigAlgos,
  }) = _AssuranceProfile;

  /// Transport tier: false is mutually authenticated, true is ratcheted.
  bool get ratcheted;

  /// Consensus tier: false is stake weighted, true is VRF leader election.
  bool get vrf;

  /// Whether validator custody must be threshold.
  bool get thresholdCustody;

  /// Whether the mempool is sealed against front running.
  bool get sealedMempool;

  /// The signature algorithms this posture admits.
  Uint8List get sigAlgos;
}
