#!/usr/bin/env bash
# Build libblockchainer_c.so for each Android ABI.
#   full (default): links CipherBird's shared libcipherbird.so (shipped by the
#                   cipherbird pub package); output -> blockchainer plugin jniLibs
#   lite:           links CipherBird's STATIC archives so the .so is self
#                   contained; output -> blockchainer_lite plugin jniLibs
# Usage: build_so.sh [full|lite]
set -euo pipefail
MODE="${1:-full}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CRYPTOLIB_ROOT="${CIPHERBIRD_ROOT:-${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}}"
CIPHERBIRD_JNILIBS="$CRYPTOLIB_ROOT/bridge/bindings/cipherbird/android/src/main/jniLibs"
CIPHERBIRD_STATIC="$CRYPTOLIB_ROOT/build-android-static"
CIPHERBIRD_DEPS="$CRYPTOLIB_ROOT/third_party/android"
case "$MODE" in
  full) PLUGIN_JNILIBS="$ROOT/bridge/bindings/blockchainer/android/src/main/jniLibs" ;;
  lite) PLUGIN_JNILIBS="$ROOT/bridge/bindings/blockchainer_lite/android/src/main/jniLibs" ;;
  *) echo "usage: $0 [full|lite]"; exit 1 ;;
esac
NDK="${ANDROID_NDK_HOME:-}"
if [[ -z "$NDK" ]]; then
  NDK="$(ls -d "$HOME/Library/Android/sdk/ndk/"* 2>/dev/null | sort -V | tail -1 || true)"
fi
[[ -n "$NDK" && -f "$NDK/build/cmake/android.toolchain.cmake" ]] || { echo "Android NDK not found (set ANDROID_NDK_HOME)"; exit 1; }
echo "NDK: $NDK  mode: $MODE"
BUILD="$ROOT/build-android-$MODE"
mkdir -p "$BUILD"
for abi in arm64-v8a x86_64; do
  if [[ "$MODE" == full ]]; then
    engine="$CIPHERBIRD_JNILIBS/$abi/libcipherbird.so"
    [[ -f "$engine" ]] || { echo "missing $engine"; exit 1; }
  else
    deps="$CIPHERBIRD_DEPS/$abi/lib"
    engine="$CIPHERBIRD_STATIC/$abi/libcryptolib_c.a"
    [[ -f "$engine" ]] || { echo "missing $engine (build CipherBird with -DCRYPTOLIB_STATIC=ON for Android)"; exit 1; }
    for dep in libsodium libblake3 liboqs libcrypto libblst libsecp256k1; do
      [[ -f "$deps/$dep.a" ]] || { echo "missing $deps/$dep.a"; exit 1; }
      engine="$engine;$deps/$dep.a"
    done
  fi
  echo "-- building $abi --"
  cmake -B "$BUILD/$abi" -S "$ROOT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-24 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
    -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" \
    -DCRYPTOLIB_C_LIBRARY="$engine" >/dev/null
  cmake --build "$BUILD/$abi" --target blockchainer_c >/dev/null
  "$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip" --strip-unneeded "$BUILD/$abi/libblockchainer_c.so"
  mkdir -p "$PLUGIN_JNILIBS/$abi"
  cp "$BUILD/$abi/libblockchainer_c.so" "$PLUGIN_JNILIBS/$abi/"
  echo "   $(du -h "$PLUGIN_JNILIBS/$abi/libblockchainer_c.so" | awk '{print $1}')  $abi/libblockchainer_c.so"
done
echo "done."
