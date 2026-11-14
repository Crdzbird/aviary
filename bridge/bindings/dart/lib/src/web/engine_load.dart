part of '../aviary_web.dart';

const _bundledEngine = 'assets/packages/aviary_dart/native/web/aviary.mjs';

/// The engine module URL, absolute: the page's `self.aviaryEngineUrl`, the
/// `AVIARY_WEB_ENGINE` define, or the asset Flutter serves.
String _engineUrl() {
  final configured = globalContext['aviaryEngineUrl'];
  if (configured.isA<JSString>()) {
    return _resolve((configured! as JSString).toDart);
  }
  const defined = String.fromEnvironment('AVIARY_WEB_ENGINE');
  if (defined.isNotEmpty) return _resolve(defined);
  return _resolve(_bundledEngine);
}

String _resolve(String url) => Uri.parse(_baseUri()).resolve(url).toString();

/// The document's base URI, which honours a base element the way Flutter's
/// own asset loading does; the location in a worker.
String _baseUri() {
  final document = globalContext['document'];
  if (document.isA<JSObject>()) {
    final base = (document! as JSObject)['baseURI'];
    if (base.isA<JSString>()) return (base! as JSString).toDart;
  }
  return Uri.base.toString();
}

/// Imports the ES module at [url], instantiates the engine it exports and
/// runs the one time native init.
Future<_Engine> _loadEngine(String url) async {
  final module = await importModule(url.toJS).toDart;
  final create = module['default'];
  if (!create.isA<JSFunction>()) {
    throw AviaryException('$url does not export the Aviary engine');
  }
  final pending = (create! as JSFunction).callAsFunction();
  final instance = await (pending! as JSPromise<JSObject>).toDart;
  final engine = _Engine(instance);
  if (engine.call('aviary_init', const []) != 0) {
    throw const AviaryException('init failed');
  }
  return engine;
}
