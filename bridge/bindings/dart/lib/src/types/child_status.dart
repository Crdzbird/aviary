part of '../../aviary.dart';

/// Whether a parent still anchors a child.
/// {@category Subchains and trees}
enum ChildStatus {
  /// The parent accepts anchors for the child.
  open(1),

  /// The child was closed: its final tip stays provable, nothing more is
  /// anchored and its chain id can never be registered again.
  closed(2);

  const ChildStatus(this.code);

  /// The ABI value.
  final int code;

  /// The status for an ABI [code]; anything but 2 reads as [open].
  static ChildStatus fromCode(int code) => code == 2 ? closed : open;
}
