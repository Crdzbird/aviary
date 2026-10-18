#!/usr/bin/env bash
# Build the self-contained macOS arm64 libaviary_c.dylib that aviary_dart, the
# npm package and the JVM jar ship, and place it where aviary_dart bundles it
# from: bridge/bindings/dart/native/darwin-arm64/libaviary_c.dylib.
#
# The CipherBird engine is linked in statically from the trimmed archive its
# iOS pipeline builds for the macosx slice (OpenSSL configured down to the EVP
# ciphers the engine uses, liboqs limited to the exposed algorithms, the small
# secp256k1 table), the same archive aviary_lite's macOS slice merges. Only the
# aviary_* functions are exported, so the result has no sibling library and no
# CipherBird symbols leak into the process.
# Usage: bash scripts/macos/build_dylib.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CRYPTOLIB_ROOT="${CIPHERBIRD_ROOT:-${CRYPTOLIB_ROOT:-$ROOT/../cryptolib}}"
ENGINE="$CRYPTOLIB_ROOT/build/ios/macosx/libcryptolib_c_merged.a"
[[ -f "$ENGINE" ]] || { echo "missing $ENGINE; run scripts/ios/build_all.sh macosx in the CipherBird checkout first"; exit 1; }
BUILD="$ROOT/build-macos"
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DAVIARY_BUILD_TESTS=OFF \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCRYPTOLIB_ROOT="$CRYPTOLIB_ROOT" \
  -DCRYPTOLIB_C_INCLUDE_DIR="$CRYPTOLIB_ROOT/bridge" \
  -DCRYPTOLIB_C_LIBRARY="$ENGINE" \
  -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-dead_strip -Wl,-exported_symbol,_aviary_* -framework Security -framework CoreFoundation" >/dev/null
cmake --build "$BUILD" --target aviary_c >/dev/null
LIB="$BUILD/libaviary_c.dylib"
install_name_tool -id "@rpath/libaviary_c.dylib" "$LIB"
strip -x "$LIB"
codesign -f -s - "$LIB" 2>/dev/null
deps="$(otool -L "$LIB" | tail -n +2 | awk '{print $1}' | grep -vE '^(/usr/lib/|/System/Library/|@rpath/libaviary_c.dylib)' || true)"
[[ -z "$deps" ]] || { echo "unexpected non-system dependencies: $deps"; exit 1; }
leaked="$(nm -gU "$LIB" | awk '{print $3}' | grep -v '^_aviary_' || true)"
[[ -z "$leaked" ]] || { echo "unexpected exported symbols: $(echo "$leaked" | head -5)"; exit 1; }
OUT="$ROOT/bridge/bindings/dart/native/darwin-arm64"
mkdir -p "$OUT"
rm -f "$OUT/libcipherbird.dylib"
cp "$LIB" "$OUT/libaviary_c.dylib"
echo "ok: $OUT/libaviary_c.dylib ($(du -h "$OUT/libaviary_c.dylib" | cut -f1), $(nm -gU "$LIB" | grep -c ' _aviary_') exported functions)"
