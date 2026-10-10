part of '../../blockchainer.dart';

final class _AssuranceProfile implements AssuranceProfile {
  const _AssuranceProfile({
    required this.ratcheted,
    required this.vrf,
    required this.thresholdCustody,
    required this.sealedMempool,
    required this.sigAlgos,
  });

  @override
  final bool ratcheted;

  @override
  final bool vrf;

  @override
  final bool thresholdCustody;

  @override
  final bool sealedMempool;

  @override
  final Uint8List sigAlgos;
}
