part of '../../aviary.dart';

/// The signature scheme of a registrar key: the transaction schemes.
/// {@category Subchains and trees}
enum SignatureScheme {
  /// Ed25519: a 32 byte public key and a 64 byte secret (seed, then public).
  ed25519(1),

  /// Hybrid Ed25519 plus ML-DSA-65, for a post quantum registrar.
  hybrid(2),

  /// SLH-DSA, the stateless hash based scheme.
  slhDsa(4);

  const SignatureScheme(this.code);

  /// The ABI value.
  final int code;
}
