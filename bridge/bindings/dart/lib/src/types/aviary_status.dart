part of '../../aviary.dart';

/// The stable status codes of the C ABI. Values never change.
enum AviaryStatus {
  /// Success.
  ok(0),

  /// A failure with no more specific mapping.
  failure(1),

  /// Genesis is sealed because a block was produced; fund accounts with
  /// transfers instead of credits.
  genesisSealed(2),

  /// Malformed input: a wrong length, a missing value or a bad encoding.
  badInput(3);

  const AviaryStatus(this.code);

  /// The ABI value.
  final int code;

  /// The status for an ABI [code]; unknown codes map to [failure].
  static AviaryStatus fromCode(int code) =>
      values.firstWhere((s) => s.code == code, orElse: () => failure);
}
