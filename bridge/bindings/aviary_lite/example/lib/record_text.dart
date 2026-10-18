import 'dart:convert';
import 'dart:typed_data';

import 'package:aviary_lite/aviary_lite.dart';

/// One line describing [record], naming the owner relative to [facilities].
String describeRecord(RecordEntry? record, Uint8List facilities) {
  if (record == null) return 'No record.';
  final who = record.owner.equalsBytes(facilities) ? 'facilities' : 'the lab';
  return 'version ${record.version}, owned by $who: ${utf8.decode(record.value)}';
}
