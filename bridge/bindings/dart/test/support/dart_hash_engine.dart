import 'dart:typed_data';

import 'package:aviary_dart/aviary.dart';

/// A consensus engine written in Dart: the justification is a tag over the
/// block's first bytes, and the counters record every crossing.
final class DartHashEngine extends ForeignEngine {
  int justified = 0;
  int verified = 0;

  static Uint8List tag(Uint8List block) {
    final n = block.length < 16 ? block.length : 16;
    return Uint8List.fromList([
      ...'dart-just:'.codeUnits,
      ...block.sublist(0, n),
    ]);
  }

  @override
  Uint8List? justify(Uint8List block) {
    justified++;
    return tag(block);
  }

  @override
  bool verify(Uint8List block, Uint8List justification) {
    verified++;
    return justification.equalsBytes(tag(block));
  }
}
