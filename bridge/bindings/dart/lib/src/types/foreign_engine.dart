part of '../../blockchainer.dart';

/// A consensus engine implemented in Dart and plugged into a mixture by id.
/// The pipeline stays native; Dart supplies the justification policy. Both
/// methods run synchronously inside consensus, must be deterministic, and
/// must not call back into the chain.
abstract class ForeignEngine {
  /// The justification for a block this node proposed, at most 4096 bytes,
  /// or null to refuse.
  Uint8List? justify(Uint8List block);

  /// Whether [justification] is valid for [block].
  bool verify(Uint8List block, Uint8List justification);
}
