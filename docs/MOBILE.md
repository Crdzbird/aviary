# Native multi-platform builds (iOS / Android / macOS / Swift / JVM)

Blockchainer's C ABI (`libblockchainer_c`) compiles to every platform and is
consumed by the language bindings. The C ABI calls the CipherBird C ABI
(`cryptolib_*` symbols, header `bridge/cryptolib_c.h` in the CipherBird repo),
so every platform build needs CipherBird's native artifact for that platform.

CipherBird lives at `../cryptolib` (git remote Crdzbird/CipherBird). Set
`CIPHERBIRD_ROOT` if it is not a sibling checkout.

## Prerequisite: CipherBird native artifacts

In the CipherBird repo:

```bash
cd ../cryptolib
bash scripts/ios/build_all.sh
bash scripts/ios/assemble_xcframework.sh
bash scripts/android/build_all.sh
```

That produces the per-slice static archives (`libcryptolib_c_merged.a`) used by
the lite packaging, the dynamic `CipherBird.xcframework` shipped by the
`cipherbird` pub package, and `libcipherbird.so` per Android ABI.

The lite Android build additionally needs CipherBird's static Android archives:

```bash
cd ../cryptolib
for abi in arm64-v8a x86_64; do
  cmake -S . -B build-android-static/$abi \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=$abi -DANDROID_PLATFORM=android-24 \
    -DCRYPTOLIB_STATIC=ON -DCRYPTOLIB_DEPS_ROOT=third_party/android/$abi \
    -DCRYPTOLIB_BUILD_EXAMPLE=OFF -DCRYPTOLIB_BUILD_TESTS=OFF
  cmake --build build-android-static/$abi --target cryptolib_c
done
```

## The two Flutter packages

| Package | Native packaging | CipherBird |
|---|---|---|
| `blockchainer_lite` | Static merged `Blockchainer.xcframework` (iOS + macOS) and a self-contained `libblockchainer_c.so` per Android ABI. | Linked inside the binary. No pub dependency. |
| `blockchainer` | Dynamic `BlockchainerC.xcframework` built with `-undefined dynamic_lookup`, and a thin `libblockchainer_c.so`. | Depends on and re-exports the `cipherbird` pub package, which ships the engine. |

Both expose the same Dart API through `blockchainer_dart`.

## Build blockchainer's native artifacts

```bash
# lite: static xcframework into bridge/bindings/blockchainer_lite/{ios,macos}
bash scripts/apple/build_xcframework.sh

# full: dynamic xcframework into bridge/bindings/blockchainer/{ios,macos}
bash scripts/apple/build_dynamic_xcframework.sh

# Android, one of:
bash scripts/android/build_so.sh lite   # self-contained .so
bash scripts/android/build_so.sh full   # links libcipherbird.so from the cipherbird package
```

The Android script strips the output with `llvm-strip --strip-unneeded`. The
lite `.so` is 8 to 9 MB per ABI; the full one is about 1 MB.

## How each binding loads it

| Binding | Native loading |
|---|---|
| Flutter `blockchainer_lite` | ffiPlugin. iOS/macOS: SPM binary target, the static archive is linked into the app and Dart uses `DynamicLibrary.process()`. Android: `DynamicLibrary.open('libblockchainer_c.so')`. |
| Flutter `blockchainer` | ffiPlugin. iOS/macOS: SPM binary target, `BlockchainerC.framework` and `CipherBird.framework` are both embedded. Android: the loader opens `libcipherbird.so` first, then `libblockchainer_c.so`. |
| Pure Dart `blockchainer_dart` | Bundles `native/darwin-arm64/{libblockchainer_c,libcipherbird}.dylib`, resolved through the package root in `package_config.json`; `BLOCKCHAINER_DYLIB` overrides. |
| Swift (`bridge/bindings/swift`) | SPM package with binary target xcframeworks (BlockchainerC + CipherBird). |
| JVM | Foreign Function and Memory API loading `libblockchainer_c` (desktop uses the host dylib). |

## Publishing notes

- pub only ships git-tracked files and honors the root `.gitignore`, so the
  binaries are force-added and each package has a `.pubignore` that re-includes
  them with `!` patterns.
- `pubspec_overrides.yaml` points `blockchainer_dart` at `../dart` for local
  development; it is excluded from the archive and the published packages
  resolve `blockchainer_dart` from pub.
- pub flattens symlinks in the archive. The macOS slice of a versioned
  framework therefore arrives with `Versions/Current` as a real directory. Xcode
  logs "Couldn't resolve framework symlink" as a warning and the build still
  succeeds; the install name points at `Versions/A`, which is unaffected.
- Android plugins must pin the Kotlin JVM target to 17 to match `compileOptions`,
  otherwise Gradle fails with "Inconsistent JVM-target compatibility".

## Why the build is simple

`blockchainer_c` is pure C++ that only needs CipherBird's header to compile.
Its `cryptolib_*` calls resolve when the final app links (iOS/macOS, or inside
the merged lite archive) or at load time from `libcipherbird.so` (full
Android). The per-platform build compiles blockchainer's sources only.
