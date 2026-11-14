/// Aviary for Dart: the whole Aviary ledger C ABI over dart:ffi, plus
/// shortcuts. In a browser the same import gives the stateless part of the
/// API over the WebAssembly engine: hashing, header hashes, and proof and
/// certificate checks.
library;

export 'src/aviary_native.dart'
    if (dart.library.js_interop) 'src/aviary_web.dart';
