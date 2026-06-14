#!/usr/bin/env bash
# Build libblockchainer_c.so for each Android ABI, linking cryptolib's Android
# .so (vendored by cryptolib_flutter). Output goes into the blockchainer_flutter
# plugin's jniLibs so consuming apps bundle it automatically.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CRYPTOLIB_ROOT="${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}"
CRYPTOLIB_JNILIBS="$CRYPTOLIB_ROOT/bridge/bindings/cryptolib_flutter/android/src/main/jniLibs"
PLUGIN_JNILIBS="$ROOT/bridge/bindings/blockchainer_flutter/android/src/main/jniLibs"

# Locate the NDK (highest version under the SDK, or $ANDROID_NDK_HOME).
NDK="${ANDROID_NDK_HOME:-}"
if [[ -z "$NDK" ]]; then
  NDK="$(ls -d "$HOME/Library/Android/sdk/ndk/"* 2>/dev/null | sort -V | tail -1 || true)"
fi
[[ -n "$NDK" && -f "$NDK/build/cmake/android.toolchain.cmake" ]] || { echo "Android NDK not found (set ANDROID_NDK_HOME)"; exit 1; }
echo "NDK: $NDK"

BUILD="$ROOT/build-android"
mkdir -p "$BUILD"

for abi in arm64-v8a x86_64; do
  local_so="$CRYPTOLIB_JNILIBS/$abi/libcryptolib_c.so"
  [[ -f "$local_so" ]] || { echo "missing cryptolib .so for $abi: $local_so"; exit 1; }
  echo "── building $abi ──"
  cmake -B "$BUILD/$abi" -S "$ROOT" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM=android-23 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
    -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" \
    -DCRYPTOLIB_C_LIBRARY="$local_so" >/dev/null
  cmake --build "$BUILD/$abi" --target blockchainer_c >/dev/null
  mkdir -p "$PLUGIN_JNILIBS/$abi"
  cp "$BUILD/$abi/libblockchainer_c.so" "$PLUGIN_JNILIBS/$abi/"
  # cryptolib's .so must sit alongside it in the consuming app.
  cp "$local_so" "$PLUGIN_JNILIBS/$abi/"
  echo "   $(du -h "$PLUGIN_JNILIBS/$abi/libblockchainer_c.so" | awk '{print $1}')  $abi/libblockchainer_c.so"
done
echo "done."
