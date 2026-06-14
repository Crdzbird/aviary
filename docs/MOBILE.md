# Native multi-platform builds (iOS / Android / macOS / Swift / JVM)

Blockchainer's C ABI (`libblockchainer_c`) compiles to every platform and is
consumed by language bindings exactly as cryptolib's is. blockchainer_c **links
cryptolib_c**, so each platform needs cryptolib's native binary for that platform.

## ⚠️ Prerequisite: cryptolib mobile binaries must be current

The interop primitives (`keccak256`, `ripemd160`, `secp256k1_*`) were added to
cryptolib's C ABI. cryptolib's **host** dylib has them, but its prebuilt
**iOS xcframework** and **Android `.so`** were built earlier and **do not**.
Until they're regenerated, blockchainer's iOS/Android builds fail to link with
`undefined symbol: cryptolib_keccak256` (and friends).

**Rebuild cryptolib's mobile binaries first** (in the cryptolib repo):

```bash
cd ../cryptolib
# iOS + macOS xcframework (rebuilds deps + slices + assembles CryptoLib.xcframework)
bash scripts/ios/build_all.sh
bash scripts/ios/assemble_xcframework.sh
# Android .so per ABI → updates cryptolib_flutter/android/src/main/jniLibs/
bash scripts/android/build_all.sh
```

Verify the symbols landed:
```bash
nm -arch arm64 CryptoLib.xcframework/ios-arm64/libcryptolib_c_merged.a | grep keccak256   # expect a hit
```

## Build blockchainer's native artifacts

Once cryptolib's mobile binaries are current:

```bash
bash scripts/build_mobile.sh
```

This produces:
- `bridge/bindings/blockchainer_flutter/ios/Blockchainer.xcframework`
- `bridge/bindings/blockchainer_flutter/macos/Blockchainer.xcframework`
- `bridge/bindings/blockchainer_flutter/android/src/main/jniLibs/<abi>/libblockchainer_c.so`
  (+ the cryptolib `.so` it depends on)

## How each binding loads it

| Binding | Native loading |
|---|---|
| **Flutter** (`bridge/bindings/blockchainer_flutter`) | ffiPlugin. iOS/macOS: the xcframework is `-force_load`ed into the app; Dart uses `DynamicLibrary.process()`. Android: bundled `.so` via `DynamicLibrary.open`. Depends on `cryptolib_flutter` for cryptolib's native. |
| **Swift** (`bridge/bindings/swift`) | SPM package with `binaryTarget` xcframeworks (Blockchainer + CryptoLib). |
| **JVM (Java/Kotlin)** | Foreign Function & Memory API loading `libblockchainer_c` (desktop uses the host dylib, which is already current). |

## iOS gotchas (already handled in the example)

Two things are required for the iOS build, both wired into `examples/blockchainer_demo`:

1. **Disable Flutter's Swift Package Manager integration** — the plugin ships CocoaPods podspecs only:
   `flutter config --no-enable-swift-package-manager` (then regenerate `ios/` if it was created with SPM on).
2. **Exclude x86_64 for the simulator** — `flutter build ios --simulator` makes a fat (arm64+x86_64) build, but the xcframework slices are **arm64-only** (so is cryptolib's). The example's `ios/Podfile` `post_install` sets `EXCLUDED_ARCHS[sdk=iphonesimulator*] = x86_64 i386` on every pod and Runner target. Apple-Silicon simulators are arm64, so this is correct.

With both in place, `flutter run -d <sim>` embeds the native (`-force_load`) and resolves it via `DynamicLibrary.process()`.

## Why the build is simple

`blockchainer_c` is pure C++ that only needs cryptolib's **header** to compile —
its `cryptolib_*` calls resolve when the final app links (iOS/macOS) or at load
time from the sibling `.so` (Android). So the per-platform build just compiles
blockchainer's sources; no dependency rebuilding (cryptolib already ships those).
