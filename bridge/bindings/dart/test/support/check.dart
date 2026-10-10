import 'package:aviary_dart/aviary.dart';

/// Throws when [ok] is false, naming the failed expectation.
void check(bool ok, String msg) {
  if (!ok) throw StateError('FAIL: $msg');
}

/// Whether [f] throws an [AviaryException].
bool fails(void Function() f) {
  try {
    f();
    return false;
  } on AviaryException catch (_) {
    return true;
  }
}

/// Whether [f] throws anything at all.
bool failsAnyhow(void Function() f) {
  try {
    f();
    return false;
  } catch (_) {
    return true;
  }
}
