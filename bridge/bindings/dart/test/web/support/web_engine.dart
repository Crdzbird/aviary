import 'dart:js_interop';
import 'dart:js_interop_unsafe';

import 'package:aviary_dart/aviary.dart';

/// The engine module as the test runner serves it: the package root sits
/// two levels above the page of a suite in test/web.
String get servedEngineUrl =>
    Uri.base.resolve('../../native/web/aviary.mjs').toString();

/// Points the loader at [url] the way a page would, through
/// `self.aviaryEngineUrl`.
void pointEngineAt(String url) {
  globalContext['aviaryEngineUrl'] = url.toJS;
}

/// Loads the engine this package ships and returns it.
Future<Aviary> loadWebEngine() async {
  pointEngineAt(servedEngineUrl);
  await Aviary.ensureInitialized();
  return Aviary.instance;
}
