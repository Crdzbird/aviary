// Blockchainer Flutter plugin (Swift Package Manager).
//
// This is an FFI plugin: there is no method channel and no native plugin class.
// The BlockchainerC binary target (a dynamic framework wrapping the prebuilt
// Blockchainer native library) is a dependency of this target purely so Swift
// Package Manager links and EMBEDS it into the host app. At runtime Dart
// resolves the C ABI via dart:ffi (DynamicLibrary.process()); Blockchainer's
// cryptolib_* symbols resolve against the CryptoLibC framework that the
// cryptolib_flutter plugin embeds. Native warm-up is driven from Dart
// (Blockchainer.preload() on a background isolate) — no native warm-up here.
import Foundation
