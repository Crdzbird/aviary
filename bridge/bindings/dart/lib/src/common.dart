/// The types and helpers that read the same on every platform: errors,
/// child records and hex. Both the dart:ffi library and the web library
/// export them.
library;

import 'dart:convert';
import 'dart:typed_data';

part 'core/exception.dart';
part 'core/exception_impl.dart';
part 'extensions/hex.dart';
part 'types/child_record.dart';
part 'types/child_record_impl.dart';
part 'types/child_status.dart';
